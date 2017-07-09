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
//: To recap: an address is a bookmark to some potentially large payload, and
//: you can replace any ingredient or product with a lookup to an address of
//: the appropriate type. But how do we get addresses to begin with? That
//: requires a little more explanation. Once we introduce the notion of
//: bookmarks to data, we have to think about the life cycle of a piece of
//: data and its bookmarks (because remember, bookmarks can be copied around
//: just like anything else). Otherwise several bad outcomes can result (and
//: indeed *have* resulted in past languages like C):
//:
//:   a) You can run out of memory if you don't have a way to reclaim
//:   data.
//:   b) If you allow data to be reclaimed, you have to be careful not to
//:   leave any stale addresses pointing at it. Otherwise your program might
//:   try to lookup such an address and find something unexpected. Such
//:   problems can be very hard to track down, and they can also be exploited
//:   to break into your computer over the network, etc.
//:
//: To avoid these problems, we introduce the notion of a *reference count* or
//: refcount. The life cycle of a bit of data accessed through addresses looks
//: like this.
//:
//:    We create space in computer memory for it using the 'new' instruction.
//:    The 'new' instruction takes a type as an ingredient, allocates
//:    sufficient space to hold that type, and returns an address (bookmark)
//:    to the allocated space.
//:
//:      x:address:num <- new number:type
//:
//:                     +------------+
//:          x -------> |  number    |
//:                     +------------+
//:
//:    That isn't entirely accurate. Under the hood, 'new' allocates an extra
//:    number -- the refcount:
//:
//:                     +------------+------------+
//:          x -------> | refcount   |  number    |
//:                     +------------+------------+
//:
//:    This probably seems like a waste of space. In practice it isn't worth
//:    allocating individual numbers and our payload will tend to be larger,
//:    so the picture would look more like this (zooming out a bit):
//:
//:                         +-------------------------+
//:                     +---+                         |
//:          x -------> | r |                         |
//:                     +---+        DATA             |
//:                         |                         |
//:                         |                         |
//:                         +-------------------------+
//:
//:    (Here 'r' denotes the refcount. It occupies a tiny amount of space
//:    compared to the payload.)
//:
//:    Anyways, back to our example where the data is just a single number.
//:    After the call to 'new', Mu's map of memory looks like this:
//:
//:                     +---+------------+
//:          x -------> | 1 |  number    |
//:                     +---+------------+
//:
//:    The refcount of 1 here indicates that this number has one bookmark
//:    outstanding. If you then make a copy of x, the refcount increments:
//:
//:      y:address:num <- copy x
//:
//:          x ---+     +---+------------+
//:               +---> | 2 |  number    |
//:          y ---+     +---+------------+
//:
//:    Whether you access the payload through x or y, Mu knows how many
//:    bookmarks are outstanding to it. When you change x or y, the refcount
//:    transparently decrements:
//:
//:      x <- copy 0  # an address is just a number, you can always write 0 to it
//:
//:                     +---+------------+
//:          y -------> | 1 |  number    |
//:                     +---+------------+
//:
//:    The final flourish is what happens when the refcount goes down to 0: Mu
//:    reclaims the space occupied by both refcount and payload in memory, and
//:    they're ready to be reused by later calls to 'new'.
//:
//:      y <- copy 0
//:
//:                     +---+------------+
//:                     | 0 |  XXXXXXX   |
//:                     +---+------------+
//:
//: Using refcounts fixes both our problems a) and b) above: you can use
//: memory for many different purposes as many times as you want without
//: running out of memory, and you don't have to worry about ever leaving a
//: dangling bookmark when you reclaim memory.
//:
//: This layer implements creating addresses using 'new'. The next few layers
//: will flesh out the rest of the life cycle.

//: todo: give 'new' a custodian ingredient. Following malloc/free is a temporary hack.

:(scenario new)
# call 'new' two times with identical types without modifying the results; you
# should get back different results
def main [
  1:address:num/raw <- new number:type
  2:address:num/raw <- new number:type
  3:bool/raw <- equal 1:address:num/raw, 2:address:num/raw
]
+mem: storing 0 in location 3

:(scenario new_array)
# call 'new' with a second ingredient to allocate an array of some type rather than a single copy
def main [
  1:address:array:num/raw <- new number:type, 5
  2:address:num/raw <- new number:type
  3:num/raw <- subtract 2:address:num/raw, 1:address:array:num/raw
]
+run: {1: ("address" "array" "number"), "raw": ()} <- new {number: "type"}, {5: "literal"}
+mem: array length is 5
# don't forget the extra location for array length, and the second extra location for the refcount
+mem: storing 7 in location 3

:(scenario dilated_reagent_with_new)
def main [
  1:address:address:num <- new {(address number): type}
]
+new: size of '(address number)' is 1

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
  if (!product.type || product.type->atom || product.type->left->value != get(Type_ordinal, "address"))
    return false;
  drop_from_type(product, "address");
  if (SIZE(inst.ingredients) > 1) {
    // array allocation
    if (!product.type || product.type->atom || product.type->left->value != get(Type_ordinal, "array"))
      return false;
    drop_from_type(product, "array");
  }
  reagent/*local*/ expected_product;
  expected_product.type = new_type_tree(inst.ingredients.at(0).name);
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

:(scenario new_returns_incorrect_type)
% Hide_errors = true;
def main [
  1:bool <- new num:type
]
+error: main: product of 'new' has incorrect type: '1:bool <- new num:type'

:(scenario new_discerns_singleton_list_from_atom_container)
% Hide_errors = true;
def main [
  1:address:num/raw <- new {(num): type}  # should be '{num: type}'
]
+error: main: product of 'new' has incorrect type: '1:address:num/raw <- new {(num): type}'

:(scenario new_with_type_abbreviation)
def main [
  1:address:num/raw <- new num:type
]
$error: 0

:(scenario new_with_type_abbreviation_inside_compound)
def main [
  {1: (address address number), raw: ()} <- new {(& num): type}
]
$error: 0

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
  trace(9991, "transform") << "--- convert 'new' to 'allocate' for recipe " << get(Recipe, r).name << end();
  for (int i = 0;  i < SIZE(get(Recipe, r).steps);  ++i) {
    instruction& inst = get(Recipe, r).steps.at(i);
    // Convert 'new' To 'allocate'
    if (inst.name == "new") {
      if (inst.ingredients.empty()) return;  // error raised elsewhere
      inst.operation = ALLOCATE;
      type_tree* type = new_type_tree(inst.ingredients.at(0).name);
      inst.ingredients.at(0).set_value(size_of(type));
      trace(9992, "new") << "size of '" << inst.ingredients.at(0).name << "' is " << inst.ingredients.at(0).value << end();
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
trace(9999, "new") << "routine allocated memory from " << alloc << " to " << alloc_max << end();

:(before "End Primitive Recipe Declarations")
ALLOCATE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "allocate", ALLOCATE);
:(before "End Primitive Recipe Implementations")
case ALLOCATE: {
  // compute the space we need
  int size = ingredients.at(0).at(0);
  if (SIZE(ingredients) > 1) {
    // array allocation
    trace(9999, "mem") << "array length is " << ingredients.at(1).at(0) << end();
    size = /*space for length*/1 + size*ingredients.at(1).at(0);
  }
  int result = allocate(size);
  if (SIZE(current_instruction().ingredients) > 1) {
    // initialize array length
    trace(9999, "mem") << "storing " << ingredients.at(1).at(0) << " in location " << result+/*skip refcount*/1 << end();
    put(Memory, result+/*skip refcount*/1, ingredients.at(1).at(0));
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}
:(code)
int allocate(int size) {
  // include space for refcount
  ++size;
  trace(9999, "mem") << "allocating size " << size << end();
//?   Total_alloc += size;
//?   ++Num_alloc;
  // Allocate Special-cases
  // compute the region of memory to return
  // really crappy at the moment
  ensure_space(size);
  const int result = Current_routine->alloc;
  trace(9999, "mem") << "new alloc: " << result << end();
  // initialize allocated space
  for (int address = result;  address < result+size;  ++address) {
    trace(9999, "mem") << "storing 0 in location " << address << end();
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
    trace(9999, "new") << "routine allocated memory from " << Current_routine->alloc << " to " << Current_routine->alloc_max << end();
  }
}

:(scenario new_initializes)
% Memory_allocated_until = 10;
% put(Memory, Memory_allocated_until, 1);
def main [
  1:address:num <- new number:type
]
+mem: storing 0 in location 10

:(scenario new_size)
def main [
  11:address:num/raw <- new number:type
  12:address:num/raw <- new number:type
  13:num/raw <- subtract 12:address:num/raw, 11:address:num/raw
]
# size of number + refcount
+mem: storing 2 in location 13

:(scenario new_array_size)
def main [
  1:address:array:num/raw <- new number:type, 5
  2:address:num/raw <- new number:type
  3:num/raw <- subtract 2:address:num/raw, 1:address:array:num/raw
]
# 5 locations for array contents + array length + refcount
+mem: storing 7 in location 3

:(scenario new_empty_array)
def main [
  1:address:array:num/raw <- new number:type, 0
  2:address:num/raw <- new number:type
  3:num/raw <- subtract 2:address:num/raw, 1:address:array:num/raw
]
+run: {1: ("address" "array" "number"), "raw": ()} <- new {number: "type"}, {0: "literal"}
+mem: array length is 0
# one location for array length, and one for the refcount
+mem: storing 2 in location 3

//: If a routine runs out of its initial allocation, it should allocate more.
:(scenario new_overflow)
% Initial_memory_per_routine = 3;  // barely enough room for point allocation below
def main [
  1:address:num/raw <- new number:type
  2:address:point/raw <- new point:type  # not enough room in initial page
]
+new: routine allocated memory from 1000 to 1003
+new: routine allocated memory from 1003 to 1006

:(scenario new_without_ingredient)
% Hide_errors = true;
def main [
  1:address:number <- new  # missing ingredient
]
+error: main: 'new' requires one or two ingredients, but got '1:address:number <- new'
