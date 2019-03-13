//: Arrays contain a variable number of elements of the same type. Their value
//: starts with the length of the array.
//:
//: You can create arrays of containers, but containers can only contain
//: elements of a fixed size, so you can't create containers containing arrays.
//: Create containers containing addresses to arrays instead.

//: You can create arrays using 'create-array'.
void test_create_array() {
  run(
      "def main [\n"
      // create an array occupying locations 1 (for the size) and 2-4 (for the elements)
      "  1:array:num:3 <- create-array\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "run: creating array from 4 locations\n"
  );
}

:(before "End Primitive Recipe Declarations")
CREATE_ARRAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "create-array", CREATE_ARRAY);
:(before "End Primitive Recipe Checks")
case CREATE_ARRAY: {
  if (inst.products.empty()) {
    raise << maybe(get(Recipe, r).name) << "'create-array' needs one product and no ingredients but got '" << to_original_string(inst) << '\n' << end();
    break;
  }
  reagent/*copy*/ product = inst.products.at(0);
  // Update CREATE_ARRAY product in Check
  if (!is_mu_array(product)) {
    raise << maybe(get(Recipe, r).name) << "'create-array' cannot create non-array '" << product.original_string << "'\n" << end();
    break;
  }
  if (!product.type->right) {
    raise << maybe(get(Recipe, r).name) << "create array of what? '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  // 'create-array' will need to check properties rather than types
  type_tree* array_length_from_type = product.type->right->right;
  if (!array_length_from_type) {
    raise << maybe(get(Recipe, r).name) << "create array of what size? '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!product.type->right->right->atom)
    array_length_from_type = array_length_from_type->left;
  if (!is_integer(array_length_from_type->name)) {
    raise << maybe(get(Recipe, r).name) << "'create-array' product should specify size of array after its element type, but got '" << product.type->right->right->name << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case CREATE_ARRAY: {
  reagent/*copy*/ product = current_instruction().products.at(0);
  // Update CREATE_ARRAY product in Run
  int base_address = product.value;
  type_tree* array_length_from_type = product.type->right->right;
  if (!product.type->right->right->atom)
    array_length_from_type = array_length_from_type->left;
  int array_length = to_integer(array_length_from_type->name);
  // initialize array length, so that size_of will work
  trace(Callstack_depth+1, "mem") << "storing " << array_length << " in location " << base_address << end();
  put(Memory, base_address, array_length);  // in array elements
  int size = size_of(product);  // in locations
  trace(Callstack_depth+1, "run") << "creating array from " << size << " locations" << end();
  // initialize array
  for (int i = 1;  i <= size_of(product);  ++i)
    put(Memory, base_address+i, 0);
  // no need to update product
  write_products = false;
  break;
}

:(code)
// Arrays can be copied around with a single instruction just like numbers,
// no matter how large they are.
// You don't need to pass the size around, since each array variable stores its
// size in memory at run-time. We'll call a variable with an explicit size a
// 'static' array, and one without a 'dynamic' array since it can contain
// arrays of many different sizes.
void test_copy_array() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  2:num <- copy 14\n"
      "  3:num <- copy 15\n"
      "  4:num <- copy 16\n"
      "  5:array:num <- copy 1:array:num:3\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 3 in location 5\n"
      "mem: storing 14 in location 6\n"
      "mem: storing 15 in location 7\n"
      "mem: storing 16 in location 8\n"
  );
}

void test_stash_array() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  2:num <- copy 14\n"
      "  3:num <- copy 15\n"
      "  4:num <- copy 16\n"
      "  stash [foo:], 1:array:num:3\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "app: foo: 3 14 15 16\n"
  );
}

:(before "End types_coercible Special-cases")
if (is_mu_array(from) && is_mu_array(to))
  return types_strictly_match(array_element(from.type), array_element(to.type));

:(before "End size_of(reagent r) Special-cases")
if (!r.type->atom && r.type->left->atom && r.type->left->value == Array_type_ordinal) {
  if (!r.type->right) {
    raise << maybe(current_recipe_name()) << "'" << r.original_string << "' is an array of what?\n" << end();
    return 1;
  }
  return /*space for length*/1 + array_length(r)*size_of(array_element(r.type));
}

:(before "End size_of(type) Non-atom Special-cases")
if (type->left->value == Array_type_ordinal) return static_array_length(type);
:(code)
int static_array_length(const type_tree* type) {
  if (!type->atom && type->right && !type->right->atom && type->right->right && !type->right->right->atom && !type->right->right->right  // exactly 3 types
      && type->right->right->left && type->right->right->left->atom && is_integer(type->right->right->left->name)) {  // third 'type' is a number
    // get size from type
    return to_integer(type->right->right->left->name);
  }
  cerr << to_string(type) << '\n';
  assert(false);
}

//: disable the size mismatch check for arrays since the destination array
//: need not be initialized
:(before "End size_mismatch(x) Special-cases")
if (x.type && !x.type->atom && x.type->left->value == Array_type_ordinal) return false;

//:: arrays inside containers
//: arrays are disallowed inside containers unless their length is fixed in
//: advance

:(code)
void test_container_permits_static_array_element() {
  run(
      "container foo [\n"
      "  x:array:num:3\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

:(before "End insert_container Special-cases")
else if (is_integer(type->name)) {  // sometimes types will contain non-type tags, like numbers for the size of an array
  type->value = 0;
}

:(code)
void test_container_disallows_dynamic_array_element() {
  Hide_errors = true;
  run(
      "container foo [\n"
      "  x:array:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: container 'foo' cannot determine size of element 'x'\n"
  );
}

:(before "End Load Container Element Definition")
{
  const type_tree* type = info.elements.back().type;
  if (type && type->atom && type->name == "array") {
    raise << "container '" << name << "' doesn't specify type of array elements for '" << info.elements.back().name << "'\n" << end();
    continue;
  }
  if (type && !type->atom && type->left->atom && type->left->name == "array") {
    if (!type->right) {
      raise << "container '" << name << "' doesn't specify type of array elements for '" << info.elements.back().name << "'\n" << end();
      continue;
    }
    if (!type->right->right || !is_integer(type->right->right->left->name)) {  // array has no length
      raise << "container '" << name << "' cannot determine size of element '" << info.elements.back().name << "'\n" << end();
      continue;
    }
  }
}

//: disable the size mismatch check for 'merge' instructions since containers
//: can contain arrays, and since we already do plenty of checking for them
:(before "End size_mismatch(x) Special-cases")
if (current_call().running_step_index < SIZE(get(Recipe, current_call().running_recipe).steps)
    && current_instruction().operation == MERGE) {
  return false;
}

:(code)
void test_merge_static_array_into_container() {
  run(
      "container foo [\n"
      "  x:num\n"
      "  y:array:num:3\n"
      "]\n"
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  10:foo <- merge 34, 1:array:num:3\n"
      "]\n"
  );
  // no errors
}

void test_code_inside_container() {
  Hide_errors = true;
  run(
      "container card [\n"
      "  rank:num <- next-ingredient\n"
      "]\n"
      "def foo [\n"
      "  1:card <- merge 3\n"
      "  2:num <- get 1:card rank:offset\n"
      "]\n"
  );
  // shouldn't die
}

//:: To access elements of an array, use 'index'

void test_index() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  2:num <- copy 14\n"
      "  3:num <- copy 15\n"
      "  4:num <- copy 16\n"
      "  10:num <- index 1:array:num:3, 0/index\n"  // the index must be a non-negative whole number
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 14 in location 10\n"
  );
}

void test_index_compound_element() {
  run(
      "def main [\n"
      "  {1: (array (address number) 3)} <- create-array\n"
      // skip alloc id
      "  3:num <- copy 14\n"
      // skip alloc id
      "  5:num <- copy 15\n"
      // skip alloc id
      "  7:num <- copy 16\n"
      "  10:address:num <- index {1: (array (address number) 3)}, 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      // skip alloc id
      "mem: storing 14 in location 11\n"
  );
}

void test_index_direct_offset() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  2:num <- copy 14\n"
      "  3:num <- copy 15\n"
      "  4:num <- copy 16\n"
      "  10:num <- copy 0\n"
      "  20:num <- index 1:array:num, 10:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 14 in location 20\n"
  );
}

:(before "End Primitive Recipe Declarations")
INDEX,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "index", INDEX);
:(before "End Primitive Recipe Checks")
case INDEX: {
  if (SIZE(inst.ingredients) != 2) {
    raise << maybe(get(Recipe, r).name) << "'index' expects exactly 2 ingredients in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  reagent/*copy*/ base = inst.ingredients.at(0);
  // Update INDEX base in Check
  if (!is_mu_array(base)) {
    raise << maybe(get(Recipe, r).name) << "'index' on a non-array '" << base.original_string << "'\n" << end();
    break;
  }
  reagent/*copy*/ index = inst.ingredients.at(1);
  // Update INDEX index in Check
  if (!is_mu_number(index)) {
    raise << maybe(get(Recipe, r).name) << "second ingredient of 'index' should be a number, but got '" << index.original_string << "'\n" << end();
    break;
  }
  if (inst.products.empty()) break;
  reagent/*copy*/ product = inst.products.at(0);
  // Update INDEX product in Check
  reagent/*local*/ element(copy_array_element(base.type));
  if (!types_coercible(product, element)) {
    raise << maybe(get(Recipe, r).name) << "'index' on '" << base.original_string << "' can't be saved in '" << product.original_string << "'; type should be '" << names_to_string_without_quotes(element.type) << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case INDEX: {
  reagent/*copy*/ base = current_instruction().ingredients.at(0);
  // Update INDEX base in Run
  int base_address = base.value;
  trace(Callstack_depth+1, "run") << "base address is " << base_address << end();
  if (base_address == 0) {
    raise << maybe(current_recipe_name()) << "tried to access location 0 in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  reagent/*copy*/ index = current_instruction().ingredients.at(1);
  // Update INDEX index in Run
  vector<double> index_val(read_memory(index));
  if (index_val.at(0) < 0 || index_val.at(0) >= get_or_insert(Memory, base_address)) {
    raise << maybe(current_recipe_name()) << "invalid index " << no_scientific(index_val.at(0)) << " in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  reagent/*local*/ element(copy_array_element(base.type));
  element.set_value(base_address + /*skip length*/1 + index_val.at(0)*size_of(element.type));
  trace(Callstack_depth+1, "run") << "address to copy is " << element.value << end();
  trace(Callstack_depth+1, "run") << "its type is " << to_string(element.type) << end();
  // Read element
  products.push_back(read_memory(element));
  break;
}

:(code)
type_tree* copy_array_element(const type_tree* type) {
  return new type_tree(*array_element(type));
}

type_tree* array_element(const type_tree* type) {
  assert(type->right);
  if (type->right->atom) {
    return type->right;
  }
  else if (!type->right->right) {
    return type->right->left;
  }
  // hack: support array:num:3 without requiring extra parens
  else if (type->right->right->left && type->right->right->left->atom && is_integer(type->right->right->left->name)) {
    assert(!type->right->right->right);
    return type->right->left;
  }
  return type->right;
}

int array_length(const reagent& x) {
  // x should already be canonized.
  // hack: look for length in type
  if (!x.type->atom && x.type->right && !x.type->right->atom && x.type->right->right && !x.type->right->right->atom && !x.type->right->right->right  // exactly 3 types
      && x.type->right->right->left && x.type->right->right->left->atom && is_integer(x.type->right->right->left->name)) {  // third 'type' is a number
    // get size from type
    return to_integer(x.type->right->right->left->name);
  }
  // this should never happen at transform time
  return get_or_insert(Memory, x.value);
}

:(before "End Unit Tests")
void test_array_length_compound() {
  put(Memory, 1, 3);
  put(Memory, 2, 14);
  put(Memory, 3, 15);
  put(Memory, 4, 16);
  reagent x("1:array:address:num");  // 3 types, but not a static array
  populate_value(x);
  CHECK_EQ(array_length(x), 3);
}

void test_array_length_static() {
  reagent x("1:array:num:3");
  CHECK_EQ(array_length(x), 3);
}

void test_index_truncates() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  2:num <- copy 14\n"
      "  3:num <- copy 15\n"
      "  4:num <- copy 16\n"
      "  10:num <- index 1:array:num:3, 1.5\n"  // non-whole number
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      // fraction is truncated away
      "mem: storing 15 in location 10\n"
  );
}

void test_index_out_of_bounds() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:array:point:3 <- create-array\n"
      "  index 1:array:point:3, 4\n"  // less than size of array in locations, but larger than its length in elements
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: invalid index 4 in 'index 1:array:point:3, 4'\n"
  );
}

void test_index_out_of_bounds_2() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  index 1:array:num, -1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: invalid index -1 in 'index 1:array:num, -1'\n"
  );
}

void test_index_product_type_mismatch() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:array:point:3 <- create-array\n"
      "  10:num <- index 1:array:point, 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: 'index' on '1:array:point' can't be saved in '10:num'; type should be 'point'\n"
  );
}

//: we might want to call 'index' without saving the results, say in a sandbox

void test_index_without_product() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  index 1:array:num:3, 0\n"
      "]\n"
  );
  // just don't die
}

//:: To write to elements of arrays, use 'put'.

void test_put_index() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  1:array:num <- put-index 1:array:num, 1, 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 3\n"
  );
}

:(before "End Primitive Recipe Declarations")
PUT_INDEX,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "put-index", PUT_INDEX);
:(before "End Primitive Recipe Checks")
case PUT_INDEX: {
  if (SIZE(inst.ingredients) != 3) {
    raise << maybe(get(Recipe, r).name) << "'put-index' expects exactly 3 ingredients in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  reagent/*copy*/ base = inst.ingredients.at(0);
  // Update PUT_INDEX base in Check
  if (!is_mu_array(base)) {
    raise << maybe(get(Recipe, r).name) << "'put-index' on a non-array '" << base.original_string << "'\n" << end();
    break;
  }
  reagent/*copy*/ index = inst.ingredients.at(1);
  // Update PUT_INDEX index in Check
  if (!is_mu_number(index)) {
    raise << maybe(get(Recipe, r).name) << "second ingredient of 'put-index' should have type 'number', but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
    break;
  }
  reagent/*copy*/ value = inst.ingredients.at(2);
  // Update PUT_INDEX value in Check
  reagent/*local*/ element(copy_array_element(base.type));
  if (!types_coercible(element, value)) {
    raise << maybe(get(Recipe, r).name) << "'put-index " << base.original_string << ", " << inst.ingredients.at(1).original_string << "' should store " << names_to_string_without_quotes(element.type) << " but '" << value.name << "' has type " << names_to_string_without_quotes(value.type) << '\n' << end();
    break;
  }
  if (inst.products.empty()) break;  // no more checks necessary
  if (inst.products.at(0).name != inst.ingredients.at(0).name) {
    raise << maybe(get(Recipe, r).name) << "product of 'put-index' must be first ingredient '" << inst.ingredients.at(0).original_string << "', but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  // End PUT_INDEX Product Checks
  break;
}
:(before "End Primitive Recipe Implementations")
case PUT_INDEX: {
  reagent/*copy*/ base = current_instruction().ingredients.at(0);
  // Update PUT_INDEX base in Run
  int base_address = base.value;
  if (base_address == 0) {
    raise << maybe(current_recipe_name()) << "tried to access location 0 in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  reagent/*copy*/ index = current_instruction().ingredients.at(1);
  // Update PUT_INDEX index in Run
  vector<double> index_val(read_memory(index));
  if (index_val.at(0) < 0 || index_val.at(0) >= get_or_insert(Memory, base_address)) {
    raise << maybe(current_recipe_name()) << "invalid index " << no_scientific(index_val.at(0)) << " in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  int address = base_address + /*skip length*/1 + index_val.at(0)*size_of(array_element(base.type));
  trace(Callstack_depth+1, "run") << "address to copy to is " << address << end();
  // optimization: directly write the element rather than updating 'product'
  // and writing the entire array
  write_products = false;
  vector<double> value = read_memory(current_instruction().ingredients.at(2));
  // Write Memory in PUT_INDEX in Run
  for (int i = 0;  i < SIZE(value);  ++i) {
    trace(Callstack_depth+1, "mem") << "storing " << no_scientific(value.at(i)) << " in location " << address+i << end();
    put(Memory, address+i, value.at(i));
  }
  break;
}

:(code)
void test_put_index_out_of_bounds() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:array:point:3 <- create-array\n"
      "  8:point <- merge 34, 35\n"
      "  1:array:point <- put-index 1:array:point, 4, 8:point\n"  // '4' is less than size of array in locations, but larger than its length in elements
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: invalid index 4 in '1:array:point <- put-index 1:array:point, 4, 8:point'\n"
  );
}

void test_put_index_out_of_bounds_2() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:array:point:3 <- create-array\n"
      "  10:point <- merge 34, 35\n"
      "  1:array:point <- put-index 1:array:point, -1, 10:point\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: invalid index -1 in '1:array:point <- put-index 1:array:point, -1, 10:point'\n"
  );
}

void test_put_index_product_error() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  4:array:num:3 <- put-index 1:array:num:3, 0, 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: product of 'put-index' must be first ingredient '1:array:num:3', but got '4:array:num:3'\n"
  );
}

//:: compute the length of an array

void test_array_length() {
  run(
      "def main [\n"
      "  1:array:num:3 <- create-array\n"
      "  10:num <- length 1:array:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 3 in location 10\n"
  );
}

:(before "End Primitive Recipe Declarations")
LENGTH,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "length", LENGTH);
:(before "End Primitive Recipe Checks")
case LENGTH: {
  if (SIZE(inst.ingredients) != 1) {
    raise << maybe(get(Recipe, r).name) << "'length' expects exactly 2 ingredients in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  reagent/*copy*/ array = inst.ingredients.at(0);
  // Update LENGTH array in Check
  if (!is_mu_array(array)) {
    raise << "tried to calculate length of non-array '" << array.original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case LENGTH: {
  reagent/*copy*/ array = current_instruction().ingredients.at(0);
  // Update LENGTH array in Run
  if (array.value == 0) {
    raise << maybe(current_recipe_name()) << "tried to access location 0 in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  products.resize(1);
  products.at(0).push_back(get_or_insert(Memory, array.value));
  break;
}

//: optimization: none of the instructions in this layer use 'ingredients' so
//: stop copying potentially huge arrays into it.
:(before "End should_copy_ingredients Special-cases")
recipe_ordinal r = current_instruction().operation;
if (r == CREATE_ARRAY || r == INDEX || r == PUT_INDEX || r == LENGTH)
  return false;
