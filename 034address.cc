//: Addresses help us spend less time copying data around.

//: So far we've been operating on primitives like numbers and characters, and
//: we've started combining these primitives together into larger logical
//: units (containers or arrays) that may contain many different primitives at
//: once. Containers and arrays can grow quite large in complex programs, and
//: we'd like some way to efficiently share them between recipes without
//: constantly having to make copies. Right now 'next-ingredient' and 'return'
//: copy data across recipe boundaries. To avoid copying large quantities of
//: data around, we'll use *addresses*. An address is a bookmark to some
//: arbitrary quantity of data (the *payload*). It's a primitive, so it's as
//: efficient to copy as a number. To read or modify the payload 'pointed to'
//: by an address, we'll perform a *lookup*.
//:
//: The notion of 'lookup' isn't an instruction like 'add' or 'subtract'.
//: Instead it's an operation that can be performed when reading any of the
//: ingredients of an instruction, and when writing to any of the products. To
//: write to the payload of an ingredient rather than its value, simply add
//: the /lookup property to it. Modern computers provide efficient support for
//: addresses and lookups, making this a realistic feature.
//:
//: To create addresses and allocate memory exclusively for their use, use
//: 'new'. Memory is a finite resource so if the computer can't satisfy your
//: request, 'new' may return a 0 (null) address.
//:
//: Computers these days have lots of memory so in practice we can often
//: assume we'll never run out. If you start running out however, say in a
//: long-running program, you'll need to switch mental gears and start
//: husbanding our memory more carefully. The most important tool to avoid
//: wasting memory is to 'abandon' an address when you don't need it anymore.
//: That frees up the memory allocated to it to be reused in future calls to
//: 'new'.

//: Since memory can be reused multiple times, it can happen that you have a
//: stale copy to an address that has since been abandoned and reused. Using
//: the stale address is almost never safe, but it can be very hard to track
//: down such copies because any errors caused by them may occur even millions
//: of instructions after the copy or abandon instruction. To help track down
//: such issues, Mu tracks an 'alloc id' for each allocation it makes. The
//: first call to 'new' has an alloc id of 1, the second gets 2, and so on.
//: The alloc id is never reused.
:(before "End Globals")
long long Next_alloc_id = 0;
:(before "End Reset")
Next_alloc_id = 0;

//: The 'new' instruction records alloc ids both in the memory being allocated
//: and *also* in the address. The 'abandon' instruction clears alloc ids in
//: both places as well. Tracking alloc ids in this manner allows us to raise
//: errors about stale addresses much earlier: 'lookup' operations always
//: compare alloc ids between the address and its payload.

//: todo: give 'new' a custodian ingredient. Following malloc/free is a temporary hack.

:(code)
void test_new() {
  run(
      // call 'new' two times with identical types without modifying the
      // results; you should get back different results
      "def main [\n"
      "  10:&:num <- new num:type\n"
      "  12:&:num <- new num:type\n"
      "  20:bool <- equal 10:&:num, 12:&:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 1000 in location 11\n"
      "mem: storing 0 in location 20\n"
  );
}

void test_new_array() {
  run(
      // call 'new' with a second ingredient to allocate an array of some type
      // rather than a single copy
      "def main [\n"
      "  10:&:@:num <- new num:type, 5\n"
      "  12:&:num <- new num:type\n"
      "  20:num/alloc2, 21:num/alloc1 <- deaddress 10:&:@:num, 12:&:num\n"
      "  30:num <- subtract 21:num/alloc2, 20:num/alloc1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "run: {10: (\"address\" \"array\" \"number\")} <- new {num: \"type\"}, {5: \"literal\"}\n"
      "mem: array length is 5\n"
      // skip alloc id in allocation
      "mem: storing 1000 in location 11\n"
      // don't forget the extra locations for alloc id and array length
      "mem: storing 7 in location 30\n"
  );
}

void test_dilated_reagent_with_new() {
  run(
      "def main [\n"
      "  10:&:&:num <- new {(& num): type}\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "new: size of '(& num)' is 2\n"
  );
}

//: 'new' takes a weird 'type' as its first ingredient; don't error on it
:(before "End Mu Types Initialization")
put(Type_ordinal, "type", 0);
:(code)
bool is_mu_type_literal(const reagent& r) {
  return is_literal(r) && r.type && r.type->name == "type";
}

:(before "End Primitive Recipe Declarations")
NEW,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "new", NEW);
:(before "End Primitive Recipe Checks")
case NEW: {
  const recipe& caller = get(Recipe, r);
  if (inst.ingredients.empty() || SIZE(inst.ingredients) > 2) {
    raise << maybe(caller.name) << "'new' requires one or two ingredients, but got '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  // End NEW Check Special-cases
  const reagent& type = inst.ingredients.at(0);
  if (!is_mu_type_literal(type)) {
    raise << maybe(caller.name) << "first ingredient of 'new' should be a type, but got '" << type.original_string << "'\n" << end();
    break;
  }
  if (SIZE(inst.ingredients) > 1 && !is_mu_number(inst.ingredients.at(1))) {
    raise << maybe(caller.name) << "second ingredient of 'new' should be a number (array length), but got '" << type.original_string << "'\n" << end();
    break;
  }
  if (inst.products.empty()) {
    raise << maybe(caller.name) << "result of 'new' should never be ignored\n" << end();
    break;
  }
  if (!product_of_new_is_valid(inst)) {
    raise << maybe(caller.name) << "product of 'new' has incorrect type: '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  break;
}

:(code)
bool product_of_new_is_valid(const instruction& inst) {
  reagent/*copy*/ product = inst.products.at(0);
  // Update NEW product in Check
  if (!product.type || product.type->atom || product.type->left->value != Address_type_ordinal)
    return false;
  drop_from_type(product, "address");
  if (SIZE(inst.ingredients) > 1) {
    // array allocation
    if (!product.type || product.type->atom || product.type->left->value != Array_type_ordinal)
      return false;
    drop_from_type(product, "array");
  }
  reagent/*local*/ expected_product(new_type_tree(inst.ingredients.at(0).name));
  return types_strictly_match(product, expected_product);
}

void drop_from_type(reagent& r, string expected_type) {
  assert(!r.type->atom);
  if (r.type->left->name != expected_type) {
    raise << "can't drop2 " << expected_type << " from '" << to_string(r) << "'\n" << end();
    return;
  }
  // r.type = r.type->right
  type_tree* tmp = r.type;
  r.type = tmp->right;
  tmp->right = NULL;
  delete tmp;
  // if (!r.type->right) r.type = r.type->left
  assert(!r.type->atom);
  if (r.type->right) return;
  tmp = r.type;
  r.type = tmp->left;
  tmp->left = NULL;
  delete tmp;
}

void test_new_returns_incorrect_type() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:bool <- new num:type\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: product of 'new' has incorrect type: '1:bool <- new num:type'\n"
  );
}

void test_new_discerns_singleton_list_from_atom_container() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:&:num <- new {(num): type}\n"  // should be '{num: type}'
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: product of 'new' has incorrect type: '1:&:num <- new {(num): type}'\n"
  );
}

void test_new_with_type_abbreviation() {
  run(
      "def main [\n"
      "  1:&:num <- new num:type\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_new_with_type_abbreviation_inside_compound() {
  run(
      "def main [\n"
      "  {1: (address address number), raw: ()} <- new {(& num): type}\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_equal_result_of_new_with_null() {
  run(
      "def main [\n"
      "  1:&:num <- new num:type\n"
      "  10:bool <- equal 1:&:num, null\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 10\n"
  );
}

//: To implement 'new', a Mu transform turns all 'new' instructions into
//: 'allocate' instructions that precompute the amount of memory they want to
//: allocate.

//: Ensure that we never call 'allocate' directly, and that there's no 'new'
//: instructions left after the transforms have run.
:(before "End Primitive Recipe Checks")
case ALLOCATE: {
  raise << "never call 'allocate' directly'; always use 'new'\n" << end();
  break;
}
:(before "End Primitive Recipe Implementations")
case NEW: {
  raise << "no implementation for 'new'; why wasn't it translated to 'allocate'? Please save a copy of your program and send it to Kartik.\n" << end();
  break;
}

:(after "Transform.push_back(check_instruction)")  // check_instruction will guard against direct 'allocate' instructions below
Transform.push_back(transform_new_to_allocate);  // idempotent

:(code)
void transform_new_to_allocate(const recipe_ordinal r) {
  trace(101, "transform") << "--- convert 'new' to 'allocate' for recipe " << get(Recipe, r).name << end();
  for (int i = 0;  i < SIZE(get(Recipe, r).steps);  ++i) {
    instruction& inst = get(Recipe, r).steps.at(i);
    // Convert 'new' To 'allocate'
    if (inst.name == "new") {
      if (inst.ingredients.empty()) return;  // error raised elsewhere
      inst.operation = ALLOCATE;
      type_tree* type = new_type_tree(inst.ingredients.at(0).name);
      inst.ingredients.at(0).set_value(size_of(type));
      trace(102, "new") << "size of '" << inst.ingredients.at(0).name << "' is " << inst.ingredients.at(0).value << end();
      delete type;
    }
  }
}

//: implement 'allocate' based on size

:(before "End Globals")
extern const int Reserved_for_tests = 1000;
int Memory_allocated_until = Reserved_for_tests;
int Initial_memory_per_routine = 100000;
:(before "End Reset")
Memory_allocated_until = Reserved_for_tests;
Initial_memory_per_routine = 100000;
:(before "End routine Fields")
int alloc, alloc_max;
:(before "End routine Constructor")
alloc = Memory_allocated_until;
Memory_allocated_until += Initial_memory_per_routine;
alloc_max = Memory_allocated_until;
trace(Callstack_depth+1, "new") << "routine allocated memory from " << alloc << " to " << alloc_max << end();

:(before "End Primitive Recipe Declarations")
ALLOCATE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "allocate", ALLOCATE);
:(before "End Primitive Recipe Implementations")
case ALLOCATE: {
  // compute the space we need
  int size = ingredients.at(0).at(0);
  int alloc_id = Next_alloc_id;
  Next_alloc_id++;
  if (SIZE(ingredients) > 1) {
    // array allocation
    trace(Callstack_depth+1, "mem") << "array length is " << ingredients.at(1).at(0) << end();
    size = /*space for length*/1 + size*ingredients.at(1).at(0);
  }
  int result = allocate(size);
  // initialize alloc-id in payload
  trace(Callstack_depth+1, "mem") << "storing alloc-id " << alloc_id << " in location " << result << end();
  put(Memory, result, alloc_id);
  if (SIZE(current_instruction().ingredients) > 1) {
    // initialize array length
    trace(Callstack_depth+1, "mem") << "storing array length " << ingredients.at(1).at(0) << " in location " << result+/*skip alloc id*/1 << end();
    put(Memory, result+/*skip alloc id*/1, ingredients.at(1).at(0));
  }
  products.resize(1);
  products.at(0).push_back(alloc_id);
  products.at(0).push_back(result);
  break;
}
:(code)
int allocate(int size) {
  // include space for alloc id
  ++size;
  trace(Callstack_depth+1, "mem") << "allocating size " << size << end();
//?   Total_alloc += size;
//?   ++Num_alloc;
  // Allocate Special-cases
  // compute the region of memory to return
  // really crappy at the moment
  ensure_space(size);
  const int result = Current_routine->alloc;
  trace(Callstack_depth+1, "mem") << "new alloc: " << result << end();
  // initialize allocated space
  for (int address = result;  address < result+size;  ++address) {
    trace(Callstack_depth+1, "mem") << "storing 0 in location " << address << end();
    put(Memory, address, 0);
  }
  Current_routine->alloc += size;
  // no support yet for reclaiming memory between routines
  assert(Current_routine->alloc <= Current_routine->alloc_max);
  return result;
}

//: statistics for debugging
//? :(before "End Globals")
//? int Total_alloc = 0;
//? int Num_alloc = 0;
//? int Total_free = 0;
//? int Num_free = 0;
//? :(before "End Reset")
//? if (!Memory.empty()) {
//?   cerr << Total_alloc << "/" << Num_alloc
//?        << " vs " << Total_free << "/" << Num_free << '\n';
//?   cerr << SIZE(Memory) << '\n';
//? }
//? Total_alloc = Num_alloc = Total_free = Num_free = 0;

:(code)
void ensure_space(int size) {
  if (size > Initial_memory_per_routine) {
    cerr << "can't allocate " << size << " locations, that's too much compared to " << Initial_memory_per_routine << ".\n";
    exit(1);
  }
  if (Current_routine->alloc + size > Current_routine->alloc_max) {
    // waste the remaining space and create a new chunk
    Current_routine->alloc = Memory_allocated_until;
    Memory_allocated_until += Initial_memory_per_routine;
    Current_routine->alloc_max = Memory_allocated_until;
    trace(Callstack_depth+1, "new") << "routine allocated memory from " << Current_routine->alloc << " to " << Current_routine->alloc_max << end();
  }
}

void test_new_initializes() {
  Memory_allocated_until = 10;
  put(Memory, Memory_allocated_until, 1);
  run(
      "def main [\n"
      "  1:&:num <- new num:type\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 10\n"
      "mem: storing 0 in location 11\n"
      "mem: storing 10 in location 2\n"
  );
}

void test_new_initializes_alloc_id() {
  Memory_allocated_until = 10;
  put(Memory, Memory_allocated_until, 1);
  Next_alloc_id = 23;
  run(
      "def main [\n"
      "  1:&:num <- new num:type\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      // initialize memory
      "mem: storing 0 in location 10\n"
      "mem: storing 0 in location 11\n"
      // alloc-id in payload
      "mem: storing alloc-id 23 in location 10\n"
      // alloc-id in address
      "mem: storing 23 in location 1\n"
  );
}

void test_new_size() {
  run(
      "def main [\n"
      "  10:&:num <- new num:type\n"
      "  12:&:num <- new num:type\n"
      "  20:num/alloc1, 21:num/alloc2 <- deaddress 10:&:num, 12:&:num\n"
      "  30:num <- subtract 21:num/alloc2, 20:num/alloc1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      // size of number + alloc id
      "mem: storing 2 in location 30\n"
  );
}

void test_new_array_size() {
  run(
      "def main [\n"
      "  10:&:@:num <- new num:type, 5\n"
      "  12:&:num <- new num:type\n"
      "  20:num/alloc1, 21:num/alloc2 <- deaddress 10:&:num, 12:&:num\n"
      "  30:num <- subtract 21:num/alloc2, 20:num/alloc1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      // 5 locations for array contents + array length + alloc id
      "mem: storing 7 in location 30\n"
  );
}

void test_new_empty_array() {
  run(
      "def main [\n"
      "  10:&:@:num <- new num:type, 0\n"
      "  12:&:num <- new num:type\n"
      "  20:num/alloc1, 21:num/alloc2 <- deaddress 10:&:@:num, 12:&:num\n"
      "  30:num <- subtract 21:num/alloc2, 20:num/alloc1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "run: {10: (\"address\" \"array\" \"number\")} <- new {num: \"type\"}, {0: \"literal\"}\n"
      "mem: array length is 0\n"
      // one location for array length and one for alloc id
      "mem: storing 2 in location 30\n"
  );
}

//: If a routine runs out of its initial allocation, it should allocate more.
void test_new_overflow() {
  Initial_memory_per_routine = 3;  // barely enough room for point allocation below
  run(
      "def main [\n"
      "  10:&:num <- new num:type\n"
      "  12:&:point <- new point:type\n"  // not enough room in initial page
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "new: routine allocated memory from 1000 to 1003\n"
      "new: routine allocated memory from 1003 to 1006\n"
  );
}

void test_new_without_ingredient() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:&:num <- new\n"  // missing ingredient
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: 'new' requires one or two ingredients, but got '1:&:num <- new'\n"
  );
}

//: a little helper: convert address to number

:(before "End Primitive Recipe Declarations")
DEADDRESS,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "deaddress", DEADDRESS);
:(before "End Primitive Recipe Checks")
case DEADDRESS: {
  // primary goal of these checks is to forbid address arithmetic
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_address(inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'deaddress' requires address ingredients, but got '" << inst.ingredients.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > SIZE(inst.ingredients)) {
    raise << maybe(get(Recipe, r).name) << "too many products in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.products);  ++i) {
    if (!is_real_mu_number(inst.products.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'deaddress' requires number products, but got '" << inst.products.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case DEADDRESS: {
  products.resize(SIZE(ingredients));
  for (int i = 0;  i < SIZE(ingredients);  ++i) {
    products.at(i).push_back(ingredients.at(i).at(/*skip alloc id*/1));
  }
  break;
}
