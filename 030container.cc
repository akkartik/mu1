//: Containers contain a fixed number of elements of different types.

:(before "End Mu Types Initialization")
//: We'll use this container as a running example in scenarios below.
type_ordinal point = put(Type_ordinal, "point", Next_type_ordinal++);
get_or_insert(Type, point);  // initialize
get(Type, point).kind = CONTAINER;
get(Type, point).name = "point";
get(Type, point).elements.push_back(reagent("x:number"));
get(Type, point).elements.push_back(reagent("y:number"));

//: Containers can be copied around with a single instruction just like
//: numbers, no matter how large they are.

//: Tests in this layer often explicitly set up memory before reading it as a
//: container. Don't do this in general. I'm tagging such cases with /unsafe;
//: they'll be exceptions to later checks.

:(code)
void test_copy_multiple_locations() {
  run(
      "def main [\n"
      "  1:num <- copy 34\n"
      "  2:num <- copy 35\n"
      "  3:point <- copy 1:point/unsafe\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 3\n"
      "mem: storing 35 in location 4\n"
  );
}

//: trying to copy to a differently-typed destination will fail
void test_copy_checks_size() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  2:point <- copy 1:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't copy '1:num' to '2:point'; types don't match\n"
  );
}

:(before "End Mu Types Initialization")
// A more complex example container, containing another container as one of
// its elements.
type_ordinal point_number = put(Type_ordinal, "point-number", Next_type_ordinal++);
get_or_insert(Type, point_number);  // initialize
get(Type, point_number).kind = CONTAINER;
get(Type, point_number).name = "point-number";
get(Type, point_number).elements.push_back(reagent("xy:point"));
get(Type, point_number).elements.push_back(reagent("z:number"));

:(code)
void test_copy_handles_nested_container_elements() {
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  15:point-number <- copy 12:point-number/unsafe\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 36 in location 17\n"
  );
}

//: products of recipes can include containers
void test_return_container() {
  run(
      "def main [\n"
      "  3:point <- f 2\n"
      "]\n"
      "def f [\n"
      "  12:num <- next-ingredient\n"
      "  13:num <- copy 35\n"
      "  return 12:point/raw\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "run: result 0 is [2, 35]\n"
      "mem: storing 2 in location 3\n"
      "mem: storing 35 in location 4\n"
  );
}

//: Containers can be checked for equality with a single instruction just like
//: numbers, no matter how large they are.

void test_compare_multiple_locations() {
  run(
      "def main [\n"
      "  1:num <- copy 34\n"  // first
      "  2:num <- copy 35\n"
      "  3:num <- copy 36\n"
      "  4:num <- copy 34\n"  // second
      "  5:num <- copy 35\n"
      "  6:num <- copy 36\n"
      "  7:bool <- equal 1:point-number/raw, 4:point-number/unsafe\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 1 in location 7\n"
  );
}

void test_compare_multiple_locations_2() {
  run(
      "def main [\n"
      "  1:num <- copy 34\n"  // first
      "  2:num <- copy 35\n"
      "  3:num <- copy 36\n"
      "  4:num <- copy 34\n"  // second
      "  5:num <- copy 35\n"
      "  6:num <- copy 37\n"  // different
      "  7:bool <- equal 1:point-number/raw, 4:point-number/unsafe\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 7\n"
  );
}

:(before "End size_of(type) Special-cases")
if (type->value == -1) return 1;  // error value, but we'll raise it elsewhere
if (type->value == 0) return 1;
if (!contains_key(Type, type->value)) {
  raise << "no such type " << type->value << '\n' << end();
  return 0;
}
type_info t = get(Type, type->value);
if (t.kind == CONTAINER) {
  // size of a container is the sum of the sizes of its elements
  int result = 0;
  for (int i = 0; i < SIZE(t.elements); ++i) {
    // todo: strengthen assertion to disallow mutual type recursion
    if (t.elements.at(i).type->value == type->value) {
      raise << "container " << t.name << " can't include itself as a member\n" << end();
      return 0;
    }
    result += size_of(element_type(type, i));
  }
  return result;
}

:(code)
void test_stash_container() {
  run(
      "def main [\n"
      "  1:num <- copy 34\n"  // first
      "  2:num <- copy 35\n"
      "  3:num <- copy 36\n"
      "  stash [foo:], 1:point-number/raw\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "app: foo: 34 35 36\n"
  );
}

//:: To access elements of a container, use 'get'
//: 'get' takes a 'base' container and an 'offset' into it and returns the
//: appropriate element of the container value.

void test_get() {
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  15:num <- get 12:point/raw, 1:offset\n"  // unsafe
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 35 in location 15\n"
  );
}

:(before "End Primitive Recipe Declarations")
GET,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "get", GET);
:(before "End Primitive Recipe Checks")
case GET: {
  if (SIZE(inst.ingredients) != 2) {
    raise << maybe(get(Recipe, r).name) << "'get' expects exactly 2 ingredients in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  reagent/*copy*/ base = inst.ingredients.at(0);  // new copy for every invocation
  // Update GET base in Check
  if (!base.type) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'get' should be a container, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  const type_tree* base_type = base.type;
  // Update GET base_type in Check
  if (!base_type->atom || base_type->value == 0 || !contains_key(Type, base_type->value) || get(Type, base_type->value).kind != CONTAINER) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'get' should be a container, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  const reagent& offset = inst.ingredients.at(1);
  if (!is_literal(offset) || !is_mu_scalar(offset)) {
    raise << maybe(get(Recipe, r).name) << "second ingredient of 'get' should have type 'offset', but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
    break;
  }
  int offset_value = 0;
  if (is_integer(offset.name)) {
    offset_value = to_integer(offset.name);
  }
  // End update GET offset_value in Check
  if (offset_value < 0 || offset_value >= SIZE(get(Type, base_type->value).elements)) {
    raise << maybe(get(Recipe, r).name) << "invalid offset '" << offset_value << "' for '" << get(Type, base_type->value).name << "'\n" << end();
    break;
  }
  if (inst.products.empty()) break;
  reagent/*copy*/ product = inst.products.at(0);
  // Update GET product in Check
  //: use base.type rather than base_type because later layers will introduce compound types
  const reagent/*copy*/ element = element_type(base.type, offset_value);
  if (!types_coercible(product, element)) {
    raise << maybe(get(Recipe, r).name) << "'get " << base.original_string << ", " << offset.original_string << "' should write to " << names_to_string_without_quotes(element.type) << " but '" << product.name << "' has type " << names_to_string_without_quotes(product.type) << '\n' << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case GET: {
  reagent/*copy*/ base = current_instruction().ingredients.at(0);
  // Update GET base in Run
  int base_address = base.value;
  if (base_address == 0) {
    raise << maybe(current_recipe_name()) << "tried to access location 0 in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  const type_tree* base_type = base.type;
  // Update GET base_type in Run
  int offset = ingredients.at(1).at(0);
  if (offset < 0 || offset >= SIZE(get(Type, base_type->value).elements)) break;  // copied from Check above
  int src = base_address;
  for (int i = 0; i < offset; ++i)
    src += size_of(element_type(base.type, i));
  trace(Callstack_depth+1, "run") << "address to copy is " << src << end();
  //: use base.type rather than base_type because later layers will introduce compound types
  reagent/*copy*/ element = element_type(base.type, offset);
  element.set_value(src);
  trace(Callstack_depth+1, "run") << "its type is " << names_to_string(element.type) << end();
  // Read element
  products.push_back(read_memory(element));
  break;
}

:(code)
const reagent element_type(const type_tree* type, int offset_value) {
  assert(offset_value >= 0);
  const type_tree* base_type = type;
  // Update base_type in element_type
  assert(contains_key(Type, base_type->value));
  assert(!get(Type, base_type->value).name.empty());
  const type_info& info = get(Type, base_type->value);
  assert(info.kind == CONTAINER);
  if (offset_value >= SIZE(info.elements)) return reagent();  // error handled elsewhere
  reagent/*copy*/ element = info.elements.at(offset_value);
  // End element_type Special-cases
  return element;
}

void test_get_handles_nested_container_elements() {
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  15:num <- get 12:point-number/raw, 1:offset\n"  // unsafe
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 36 in location 15\n"
  );
}

void test_get_out_of_bounds() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  get 12:point-number/raw, 2:offset\n"  // point-number occupies 3 locations but has only 2 fields; out of bounds
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: invalid offset '2' for 'point-number'\n"
  );
}

void test_get_out_of_bounds_2() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  get 12:point-number/raw, -1:offset\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: invalid offset '-1' for 'point-number'\n"
  );
}

void test_get_product_type_mismatch() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  15:&:num <- get 12:point-number/raw, 1:offset\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: 'get 12:point-number/raw, 1:offset' should write to number but '15' has type (address number)\n"
  );
}

//: we might want to call 'get' without saving the results, say in a sandbox

void test_get_without_product() {
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  get 12:point/raw, 1:offset\n"  // unsafe
      "]\n"
  );
  // just don't die
}

//:: To write to elements of containers, use 'put'.

void test_put() {
  run(
      "def main [\n"
      "  12:num <- copy 34\n"
      "  13:num <- copy 35\n"
      "  $clear-trace\n"
      "  12:point <- put 12:point, 1:offset, 36\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 36 in location 13"
  );
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 34 in location 12");
}

:(before "End Primitive Recipe Declarations")
PUT,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "put", PUT);
:(before "End Primitive Recipe Checks")
case PUT: {
  if (SIZE(inst.ingredients) != 3) {
    raise << maybe(get(Recipe, r).name) << "'put' expects exactly 3 ingredients in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  reagent/*copy*/ base = inst.ingredients.at(0);
  // Update PUT base in Check
  if (!base.type) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'put' should be a container, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  const type_tree* base_type = base.type;
  // Update PUT base_type in Check
  if (!base_type->atom || base_type->value == 0 || !contains_key(Type, base_type->value) || get(Type, base_type->value).kind != CONTAINER) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'put' should be a container, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  reagent/*copy*/ offset = inst.ingredients.at(1);
  // Update PUT offset in Check
  if (!is_literal(offset) || !is_mu_scalar(offset)) {
    raise << maybe(get(Recipe, r).name) << "second ingredient of 'put' should have type 'offset', but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
    break;
  }
  int offset_value = 0;
  //: later layers will permit non-integer offsets
  if (is_integer(offset.name)) {
    offset_value = to_integer(offset.name);
    if (offset_value < 0 || offset_value >= SIZE(get(Type, base_type->value).elements)) {
      raise << maybe(get(Recipe, r).name) << "invalid offset '" << offset_value << "' for '" << get(Type, base_type->value).name << "'\n" << end();
      break;
    }
  }
  else {
    offset_value = offset.value;
  }
  const reagent& value = inst.ingredients.at(2);
  //: use base.type rather than base_type because later layers will introduce compound types
  const reagent& element = element_type(base.type, offset_value);
  if (!types_coercible(element, value)) {
    raise << maybe(get(Recipe, r).name) << "'put " << base.original_string << ", " << offset.original_string << "' should write to " << names_to_string_without_quotes(element.type) << " but '" << value.name << "' has type " << names_to_string_without_quotes(value.type) << '\n' << end();
    break;
  }
  if (inst.products.empty()) break;  // no more checks necessary
  if (inst.products.at(0).name != inst.ingredients.at(0).name) {
    raise << maybe(get(Recipe, r).name) << "product of 'put' must be first ingredient '" << inst.ingredients.at(0).original_string << "', but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  // End PUT Product Checks
  break;
}
:(before "End Primitive Recipe Implementations")
case PUT: {
  reagent/*copy*/ base = current_instruction().ingredients.at(0);
  // Update PUT base in Run
  int base_address = base.value;
  if (base_address == 0) {
    raise << maybe(current_recipe_name()) << "tried to access location 0 in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  const type_tree* base_type = base.type;
  // Update PUT base_type in Run
  int offset = ingredients.at(1).at(0);
  if (offset < 0 || offset >= SIZE(get(Type, base_type->value).elements)) break;  // copied from Check above
  int address = base_address;
  for (int i = 0; i < offset; ++i)
    address += size_of(element_type(base.type, i));
  trace(Callstack_depth+1, "run") << "address to copy to is " << address << end();
  // optimization: directly write the element rather than updating 'product'
  // and writing the entire container
  // Write Memory in PUT in Run
  write_products = false;
  for (int i = 0;  i < SIZE(ingredients.at(2));  ++i) {
    trace(Callstack_depth+1, "mem") << "storing " << no_scientific(ingredients.at(2).at(i)) << " in location " << address+i << end();
    put(Memory, address+i, ingredients.at(2).at(i));
  }
  break;
}

:(code)
void test_put_product_error() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  1:point <- merge 34, 35\n"
      "  3:point <- put 1:point, x:offset, 36\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: product of 'put' must be first ingredient '1:point', but got '3:point'\n"
  );
}

//:: Allow containers to be defined in Mu code.

void test_container() {
  load(
      "container foo [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "parse: --- defining container foo\n"
      "parse: element: {x: \"number\"}\n"
      "parse: element: {y: \"number\"}\n"
  );
}

void test_container_use_before_definition() {
  load(
      "container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "container bar [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "parse: --- defining container foo\n"
      "parse: type number: 1000\n"
      "parse:   element: {x: \"number\"}\n"
      // todo: brittle
      // type bar is unknown at this point, but we assign it a number
      "parse:   element: {y: \"bar\"}\n"
      // later type bar gets a definition
      "parse: --- defining container bar\n"
      "parse: type number: 1001\n"
      "parse:   element: {x: \"number\"}\n"
      "parse:   element: {y: \"number\"}\n"
  );
}

//: if a container is defined again, the new fields add to the original definition
void test_container_extend() {
  run(
      "container foo [\n"
      "  x:num\n"
      "]\n"
      "container foo [\n"  // add to previous definition
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:num <- copy 34\n"
      "  2:num <- copy 35\n"
      "  3:num <- get 1:foo, 0:offset\n"
      "  4:num <- get 1:foo, 1:offset\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 3\n"
      "mem: storing 35 in location 4\n"
  );
}

:(before "End Command Handlers")
else if (command == "container") {
  insert_container(command, CONTAINER, in);
}

//: Even though we allow containers to be extended, we don't allow this after
//: a call to transform_all. But we do want to detect this situation and raise
//: an error. This field will help us raise such errors.
:(before "End type_info Fields")
int Num_calls_to_transform_all_at_first_definition;
:(before "End type_info Constructor")
Num_calls_to_transform_all_at_first_definition = -1;

:(code)
void insert_container(const string& command, kind_of_type kind, istream& in) {
  skip_whitespace_but_not_newline(in);
  string name = next_word(in);
  if (name.empty()) {
    assert(!has_data(in));
    raise << "incomplete container definition at end of file (0)\n" << end();
    return;
  }
  // End container Name Refinements
  trace(101, "parse") << "--- defining " << command << ' ' << name << end();
  if (!contains_key(Type_ordinal, name)
      || get(Type_ordinal, name) == 0) {
    put(Type_ordinal, name, Next_type_ordinal++);
  }
  trace(102, "parse") << "type number: " << get(Type_ordinal, name) << end();
  skip_bracket(in, "'"+command+"' must begin with '['");
  type_info& info = get_or_insert(Type, get(Type_ordinal, name));
  if (info.Num_calls_to_transform_all_at_first_definition == -1) {
    // initial definition of this container
    info.Num_calls_to_transform_all_at_first_definition = Num_calls_to_transform_all;
  }
  else if (info.Num_calls_to_transform_all_at_first_definition != Num_calls_to_transform_all) {
    // extension after transform_all
    raise << "there was a call to transform_all() between the definition of container '" << name << "' and a subsequent extension. This is not supported, since any recipes that used '" << name << "' values have already been transformed and \"frozen\".\n" << end();
    return;
  }
  info.name = name;
  info.kind = kind;
  while (has_data(in)) {
    skip_whitespace_and_comments(in);
    string element = next_word(in);
    if (element.empty()) {
      assert(!has_data(in));
      raise << "incomplete container definition at end of file (1)\n" << end();
      return;
    }
    if (element == "]") break;
    if (in.peek() != '\n') {
      raise << command << " '" << name << "' contains multiple elements on a single line. Containers and exclusive containers must only contain elements, one to a line, no code.\n" << end();
      // skip rest of container declaration
      while (has_data(in)) {
        skip_whitespace_and_comments(in);
        if (next_word(in) == "]") break;
      }
      break;
    }
    info.elements.push_back(reagent(element));
    expand_type_abbreviations(info.elements.back().type);  // todo: use abbreviation before declaration
    replace_unknown_types_with_unique_ordinals(info.elements.back().type, info);
    trace(103, "parse") << "  element: " << to_string(info.elements.back()) << end();
    // End Load Container Element Definition
  }
}

void replace_unknown_types_with_unique_ordinals(type_tree* type, const type_info& info) {
  if (!type) return;
  if (!type->atom) {
    replace_unknown_types_with_unique_ordinals(type->left, info);
    replace_unknown_types_with_unique_ordinals(type->right, info);
    return;
  }
  assert(!type->name.empty());
  if (contains_key(Type_ordinal, type->name)) {
    type->value = get(Type_ordinal, type->name);
  }
  // End insert_container Special-cases
  else if (type->name != "->") {  // used in recipe types
    put(Type_ordinal, type->name, Next_type_ordinal++);
    type->value = get(Type_ordinal, type->name);
  }
}

void skip_bracket(istream& in, string message) {
  skip_whitespace_and_comments(in);
  if (in.get() != '[')
    raise << message << '\n' << end();
}

void test_multi_word_line_in_container_declaration() {
  Hide_errors = true;
  run(
      "container foo [\n"
      "  x:num y:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: container 'foo' contains multiple elements on a single line. Containers and exclusive containers must only contain elements, one to a line, no code.\n"
  );
}

//: support type abbreviations in container definitions

void test_type_abbreviations_in_containers() {
  run(
      "type foo = number\n"
      "container bar [\n"
      "  x:foo\n"
      "]\n"
      "def main [\n"
      "  1:num <- copy 34\n"
      "  2:foo <- get 1:bar/unsafe, 0:offset\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 2\n"
  );
}

:(after "Transform.push_back(expand_type_abbreviations)")
Transform.push_back(expand_type_abbreviations_in_containers);  // idempotent
:(code)
// extremely inefficient; we process all types over and over again, once for every single recipe
// but it doesn't seem to cause any noticeable slowdown
void expand_type_abbreviations_in_containers(const recipe_ordinal /*unused*/) {
  for (map<type_ordinal, type_info>::iterator p = Type.begin();  p != Type.end();  ++p) {
    for (int i = 0;  i < SIZE(p->second.elements);  ++i)
      expand_type_abbreviations(p->second.elements.at(i).type);
  }
}

//: ensure scenarios are consistent by always starting new container
//: declarations at the same type number
:(before "End Reset")  //: for tests
Next_type_ordinal = 1000;
:(before "End Test Run Initialization")
assert(Next_type_ordinal < 1000);

:(code)
void test_error_on_transform_all_between_container_definition_and_extension() {
  // define a container
  run("container foo [\n"
      "  a:num\n"
      "]\n");
  // try to extend the container after transform
  transform_all();
  CHECK_TRACE_DOESNT_CONTAIN_ERRORS();
  Hide_errors = true;
  run("container foo [\n"
      "  b:num\n"
      "]\n");
  CHECK_TRACE_CONTAINS_ERRORS();
}

//:: Allow container definitions anywhere in the codebase, but complain if you
//:: can't find a definition at the end.

void test_run_complains_on_unknown_types() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:integer <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: unknown type integer in '1:integer <- copy 0'\n"
  );
}

void test_run_allows_type_definition_after_use() {
  run(
      "def main [\n"
      "  1:bar <- copy 0/unsafe\n"
      "]\n"
      "container bar [\n"
      "  x:num\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

:(before "End Type Modifying Transforms")
Transform.push_back(check_or_set_invalid_types);  // idempotent

:(code)
void check_or_set_invalid_types(const recipe_ordinal r) {
  recipe& caller = get(Recipe, r);
  trace(101, "transform") << "--- check for invalid types in recipe " << caller.name << end();
  for (int index = 0;  index < SIZE(caller.steps);  ++index) {
    instruction& inst = caller.steps.at(index);
    for (int i = 0;  i < SIZE(inst.ingredients);  ++i)
      check_or_set_invalid_types(inst.ingredients.at(i), caller, inst);
    for (int i = 0;  i < SIZE(inst.products);  ++i)
      check_or_set_invalid_types(inst.products.at(i), caller, inst);
  }
  // End check_or_set_invalid_types
}

void check_or_set_invalid_types(reagent& r, const recipe& caller, const instruction& inst) {
  // Begin check_or_set_invalid_types(r)
  check_or_set_invalid_types(r.type, maybe(caller.name), "'"+to_original_string(inst)+"'");
}

void check_or_set_invalid_types(type_tree* type, const string& location_for_error_messages, const string& name_for_error_messages) {
  if (!type) return;
  // End Container Type Checks
  if (!type->atom) {
    check_or_set_invalid_types(type->left, location_for_error_messages, name_for_error_messages);
    check_or_set_invalid_types(type->right, location_for_error_messages, name_for_error_messages);
    return;
  }
  if (type->value == 0) return;
  if (!contains_key(Type, type->value)) {
    assert(!type->name.empty());
    if (contains_key(Type_ordinal, type->name))
      type->value = get(Type_ordinal, type->name);
    else
      raise << location_for_error_messages << "unknown type " << type->name << " in " << name_for_error_messages << '\n' << end();
  }
}

void test_container_unknown_field() {
  Hide_errors = true;
  run(
      "container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: foo: unknown type in y\n"
  );
}

void test_read_container_with_bracket_in_comment() {
  run(
      "container foo [\n"
      "  x:num\n"
      "  # ']' in comment\n"
      "  y:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "parse: --- defining container foo\n"
      "parse: element: {x: \"number\"}\n"
      "parse: element: {y: \"number\"}\n"
  );
}
void test_container_with_compound_field_type() {
  run(
      "container foo [\n"
      "  {x: (address array (address array character))}\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

:(before "End transform_all")
check_container_field_types();

:(code)
void check_container_field_types() {
  for (map<type_ordinal, type_info>::iterator p = Type.begin();  p != Type.end();  ++p) {
    const type_info& info = p->second;
    // Check Container Field Types(info)
    for (int i = 0;  i < SIZE(info.elements);  ++i)
      check_invalid_types(info.elements.at(i).type, maybe(info.name), info.elements.at(i).name);
  }
}

void check_invalid_types(const type_tree* type, const string& location_for_error_messages, const string& name_for_error_messages) {
  if (!type) return;  // will throw a more precise error elsewhere
  if (!type->atom) {
    check_invalid_types(type->left, location_for_error_messages, name_for_error_messages);
    check_invalid_types(type->right, location_for_error_messages, name_for_error_messages);
    return;
  }
  if (type->value != 0) {  // value 0 = compound types (layer parse_tree) or type ingredients (layer shape_shifting_container)
    if (!contains_key(Type, type->value))
      raise << location_for_error_messages << "unknown type in " << name_for_error_messages << '\n' << end();
  }
}

string to_original_string(const type_ordinal t) {
  ostringstream out;
  if (!contains_key(Type, t)) return out.str();
  const type_info& info = get(Type, t);
  if (info.kind == PRIMITIVE) return out.str();
  out << (info.kind == CONTAINER ? "container" : "exclusive-container") << " " << info.name << " [\n";
  for (int i = 0;  i < SIZE(info.elements);  ++i) {
    out << "  " << info.elements.at(i).original_string << "\n";
  }
  out << "]\n";
  return out.str();
}
