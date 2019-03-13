//: Exclusive containers contain exactly one of a fixed number of 'variants'
//: of different types.
//:
//: They also implicitly contain a tag describing precisely which variant is
//: currently stored in them.

:(before "End Mu Types Initialization")
//: We'll use this container as a running example, with two number elements.
{
type_ordinal tmp = put(Type_ordinal, "number-or-point", Next_type_ordinal++);
get_or_insert(Type, tmp);  // initialize
get(Type, tmp).kind = EXCLUSIVE_CONTAINER;
get(Type, tmp).name = "number-or-point";
get(Type, tmp).elements.push_back(reagent("i:number"));
get(Type, tmp).elements.push_back(reagent("p:point"));
}

//: Tests in this layer often explicitly set up memory before reading it as a
//: container. Don't do this in general. I'm tagging such cases with /unsafe;
//: they'll be exceptions to later checks.

:(code)
void test_copy_exclusive_container() {
  run(
      // Copying exclusive containers copies all their contents, and an extra
      // location for the tag.
      "def main [\n"
      "  1:num <- copy 1\n"  // 'point' variant
      "  2:num <- copy 34\n"
      "  3:num <- copy 35\n"
      "  4:number-or-point <- copy 1:number-or-point/unsafe\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 1 in location 4\n"
      "mem: storing 34 in location 5\n"
      "mem: storing 35 in location 6\n"
  );
}

:(before "End size_of(type) Special-cases")
if (t.kind == EXCLUSIVE_CONTAINER) {
  // size of an exclusive container is the size of its largest variant
  // (So like containers, it can't contain arrays.)
  int result = 0;
  for (int i = 0; i < SIZE(t.elements); ++i) {
    reagent tmp;
    tmp.type = new type_tree(*type);
    int size = size_of(variant_type(tmp, i));
    if (size > result) result = size;
  }
  // ...+1 for its tag.
  return result+1;
}

//:: To access variants of an exclusive container, use 'maybe-convert'.
//: It always returns an address (so that you can modify it) or null (to
//: signal that the conversion failed (because the container contains a
//: different variant).

//: 'maybe-convert' requires a literal in ingredient 1. We'll use a synonym
//: called 'variant'.
:(before "End Mu Types Initialization")
put(Type_ordinal, "variant", 0);

:(code)
void test_maybe_convert() {
  run(
      "def main [\n"
      "  12:num <- copy 1\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  20:point, 22:bool <- maybe-convert 12:number-or-point/unsafe, 1:variant\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      // boolean
      "mem: storing 1 in location 22\n"
      // point
      "mem: storing 35 in location 20\n"
      "mem: storing 36 in location 21\n"
  );
}

void test_maybe_convert_fail() {
  run(
      "def main [\n"
      "  12:num <- copy 1\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  20:num, 21:bool <- maybe-convert 12:number-or-point/unsafe, 0:variant\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      // boolean
      "mem: storing 0 in location 21\n"
      // number: no write
  );
}


:(before "End Primitive Recipe Declarations")
MAYBE_CONVERT,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "maybe-convert", MAYBE_CONVERT);
:(before "End Primitive Recipe Checks")
case MAYBE_CONVERT: {
  const recipe& caller = get(Recipe, r);
  if (SIZE(inst.ingredients) != 2) {
    raise << maybe(caller.name) << "'maybe-convert' expects exactly 2 ingredients in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  reagent/*copy*/ base = inst.ingredients.at(0);
  // Update MAYBE_CONVERT base in Check
  if (!base.type) {
    raise << maybe(caller.name) << "first ingredient of 'maybe-convert' should be an exclusive-container, but got '" << base.original_string << "'\n" << end();
    break;
  }
  const type_tree* base_type = base.type;
  // Update MAYBE_CONVERT base_type in Check
  if (!base_type->atom || base_type->value == 0 || !contains_key(Type, base_type->value) || get(Type, base_type->value).kind != EXCLUSIVE_CONTAINER) {
    raise << maybe(caller.name) << "first ingredient of 'maybe-convert' should be an exclusive-container, but got '" << base.original_string << "'\n" << end();
    break;
  }
  if (!is_literal(inst.ingredients.at(1))) {
    raise << maybe(caller.name) << "second ingredient of 'maybe-convert' should have type 'variant', but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
    break;
  }
  if (inst.products.empty()) break;
  if (SIZE(inst.products) != 2) {
    raise << maybe(caller.name) << "'maybe-convert' expects exactly 2 products in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  reagent/*copy*/ product = inst.products.at(0);
  // Update MAYBE_CONVERT product in Check
  reagent& offset = inst.ingredients.at(1);
  populate_value(offset);
  if (offset.value >= SIZE(get(Type, base_type->value).elements)) {
    raise << maybe(caller.name) << "invalid tag " << offset.value << " in '" << to_original_string(inst) << '\n' << end();
    break;
  }
  const reagent& variant = variant_type(base, offset.value);
  if (!types_coercible(product, variant)) {
    raise << maybe(caller.name) << "'maybe-convert " << base.original_string << ", " << inst.ingredients.at(1).original_string << "' should write to " << to_string(variant.type) << " but '" << product.name << "' has type " << to_string(product.type) << '\n' << end();
    break;
  }
  reagent/*copy*/ status = inst.products.at(1);
  // Update MAYBE_CONVERT status in Check
  if (!is_mu_boolean(status)) {
    raise << maybe(get(Recipe, r).name) << "second product yielded by 'maybe-convert' should be a boolean, but tried to write to '" << inst.products.at(1).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case MAYBE_CONVERT: {
  reagent/*copy*/ base = current_instruction().ingredients.at(0);
  // Update MAYBE_CONVERT base in Run
  int base_address = base.value;
  if (base_address == 0) {
    raise << maybe(current_recipe_name()) << "tried to access location 0 in '" << to_original_string(current_instruction()) << "'\n" << end();
    break;
  }
  int tag = current_instruction().ingredients.at(1).value;
  reagent/*copy*/ product = current_instruction().products.at(0);
  // Update MAYBE_CONVERT product in Run
  reagent/*copy*/ status = current_instruction().products.at(1);
  // Update MAYBE_CONVERT status in Run
  // optimization: directly write results to only update first product when necessary
  write_products = false;
  if (tag == static_cast<int>(get_or_insert(Memory, base_address))) {
    const reagent& variant = variant_type(base, tag);
    trace(Callstack_depth+1, "mem") << "storing 1 in location " << status.value << end();
    put(Memory, status.value, 1);
    if (!is_dummy(product)) {
      // Write Memory in Successful MAYBE_CONVERT in Run
      for (int i = 0;  i < size_of(variant);  ++i) {
        double val = get_or_insert(Memory, base_address+/*skip tag*/1+i);
        trace(Callstack_depth+1, "mem") << "storing " << no_scientific(val) << " in location " << product.value+i << end();
        put(Memory, product.value+i, val);
      }
    }
  }
  else {
    trace(Callstack_depth+1, "mem") << "storing 0 in location " << status.value << end();
    put(Memory, status.value, 0);
  }
  break;
}

:(code)
const reagent variant_type(const reagent& base, int tag) {
  return variant_type(base.type, tag);
}

const reagent variant_type(const type_tree* type, int tag) {
  assert(tag >= 0);
  const type_tree* root_type = type->atom ? type : type->left;
  assert(contains_key(Type, root_type->value));
  assert(!get(Type, root_type->value).name.empty());
  const type_info& info = get(Type, root_type->value);
  assert(info.kind == EXCLUSIVE_CONTAINER);
  reagent/*copy*/ element = info.elements.at(tag);
  // End variant_type Special-cases
  return element;
}

void test_maybe_convert_product_type_mismatch() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  12:num <- copy 1\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  20:num, 21:bool <- maybe-convert 12:number-or-point/unsafe, 1:variant\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: 'maybe-convert 12:number-or-point/unsafe, 1:variant' should write to point but '20' has type number\n"
  );
}

void test_maybe_convert_dummy_product() {
  run(
      "def main [\n"
      "  12:num <- copy 1\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  _, 21:bool <- maybe-convert 12:number-or-point/unsafe, 1:variant\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

//:: Allow exclusive containers to be defined in Mu code.

void test_exclusive_container() {
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "parse: --- defining exclusive-container foo\n"
      "parse: element: {x: \"number\"}\n"
      "parse: element: {y: \"number\"}\n"
  );
}

:(before "End Command Handlers")
else if (command == "exclusive-container") {
  insert_container(command, EXCLUSIVE_CONTAINER, in);
}

//: arrays are disallowed inside exclusive containers unless their length is
//: fixed in advance

:(code)
void test_exclusive_container_contains_array() {
  run(
      "exclusive-container foo [\n"
      "  x:@:num:3\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_exclusive_container_disallows_dynamic_array_element() {
  Hide_errors = true;
  run(
      "exclusive-container foo [\n"
      "  x:@:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: container 'foo' cannot determine size of element 'x'\n"
  );
}

//:: To construct exclusive containers out of variant types, use 'merge'.
void test_lift_to_exclusive_container() {
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:num <- copy 34\n"
      "  2:foo <- merge 0/x, 1:num\n"  // tag must be a literal when merging exclusive containers
      "  4:foo <- merge 1/y, 1:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 2\n"
      "mem: storing 34 in location 3\n"
      "mem: storing 1 in location 4\n"
      "mem: storing 34 in location 5\n"
  );
}

//: type-checking for 'merge' on exclusive containers

void test_merge_handles_exclusive_container() {
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "container bar [\n"
      "  z:num\n"
      "]\n"
      "def main [\n"
      "  1:foo <- merge 0/x, 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 1\n"
      "mem: storing 34 in location 2\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_merge_requires_literal_tag_for_exclusive_container() {
  Hide_errors = true;
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "container bar [\n"
      "  z:num\n"
      "]\n"
      "def main [\n"
      "  1:num <- copy 0\n"
      "  2:foo <- merge 1:num, 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: ingredient 0 of 'merge' should be a literal, for the tag of exclusive-container 'foo' in '2:foo <- merge 1:num, 34'\n"
  );
}

void test_merge_handles_exclusive_container_inside_exclusive_container() {
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "exclusive-container bar [\n"
      "  a:num\n"
      "  b:num\n"
      "]\n"
      "def main [\n"
      "  1:num <- copy 0\n"
      "  2:bar <- merge 0/a, 34\n"
      "  4:foo <- merge 1/y, 2:bar\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 5\n"
      "mem: storing 34 in location 6\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

:(before "End check_merge_call Special-cases")
case EXCLUSIVE_CONTAINER: {
  assert(state.data.top().container_element_index == 0);
  trace(102, "transform") << "checking exclusive container " << to_string(container) << " vs ingredient " << ingredient_index << end();
  // easy case: exact match
  if (types_strictly_match(container, inst.ingredients.at(ingredient_index)))
    return;
  if (!is_literal(ingredients.at(ingredient_index))) {
    raise << maybe(caller.name) << "ingredient " << ingredient_index << " of 'merge' should be a literal, for the tag of exclusive-container '" << container_info.name << "' in '" << to_original_string(inst) << "'\n" << end();
    return;
  }
  reagent/*copy*/ ingredient = ingredients.at(ingredient_index);  // unnecessary copy just to keep this function from modifying caller
  populate_value(ingredient);
  if (ingredient.value >= SIZE(container_info.elements)) {
    raise << maybe(caller.name) << "invalid tag at " << ingredient_index << " for '" << container_info.name << "' in '" << to_original_string(inst) << "'\n" << end();
    return;
  }
  const reagent& variant = variant_type(container, ingredient.value);
  trace(102, "transform") << "tag: " << ingredient.value << end();
  // replace union with its variant
  state.data.pop();
  state.data.push(merge_check_point(variant, 0));
  ++ingredient_index;
  break;
}

:(code)
void test_merge_check_container_containing_exclusive_container() {
  run(
      "container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "exclusive-container bar [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:foo <- merge 23, 1/y, 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 23 in location 1\n"
      "mem: storing 1 in location 2\n"
      "mem: storing 34 in location 3\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_merge_check_container_containing_exclusive_container_2() {
  Hide_errors = true;
  run(
      "container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "exclusive-container bar [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:foo <- merge 23, 1/y, 34, 35\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: too many ingredients in '1:foo <- merge 23, 1/y, 34, 35'\n"
  );
}

void test_merge_check_exclusive_container_containing_container() {
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "container bar [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:foo <- merge 1/y, 23, 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 1 in location 1\n"
      "mem: storing 23 in location 2\n"
      "mem: storing 34 in location 3\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_merge_check_exclusive_container_containing_container_2() {
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "container bar [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:foo <- merge 0/x, 23\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_merge_check_exclusive_container_containing_container_3() {
  Hide_errors = true;
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "container bar [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "def main [\n"
      "  1:foo <- merge 1/y, 23\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: too few ingredients in '1:foo <- merge 1/y, 23'\n"
  );
}

void test_merge_check_exclusive_container_containing_container_4() {
  run(
      "exclusive-container foo [\n"
      "  x:num\n"
      "  y:bar\n"
      "]\n"
      "container bar [\n"
      "  a:num\n"
      "  b:num\n"
      "]\n"
      "def main [\n"
      "  1:bar <- merge 23, 24\n"
      "  3:foo <- merge 1/y, 1:bar\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

//: Since the different variants of an exclusive-container might have
//: different sizes, relax the size mismatch check for 'merge' instructions.
:(before "End size_mismatch(x) Special-cases")
if (current_step_index() < SIZE(Current_routine->steps())
    && current_instruction().operation == MERGE
    && !current_instruction().products.empty()
    && current_instruction().products.at(0).type) {
  reagent/*copy*/ x = current_instruction().products.at(0);
  // Update size_mismatch Check for MERGE(x)
  const type_tree* root_type = x.type->atom ? x.type : x.type->left;
  assert(root_type->atom);
  if (get(Type, root_type->value).kind == EXCLUSIVE_CONTAINER)
    return size_of(x) < SIZE(data);
}

:(code)
void test_merge_exclusive_container_with_mismatched_sizes() {
  run(
      "container foo [\n"
      "  x:num\n"
      "  y:num\n"
      "]\n"
      "exclusive-container bar [\n"
      "  x:num\n"
      "  y:foo\n"
      "]\n"
      "def main [\n"
      "  1:num <- copy 34\n"
      "  2:num <- copy 35\n"
      "  3:bar <- merge 0/x, 1:num\n"
      "  6:bar <- merge 1/foo, 1:num, 2:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 3\n"
      "mem: storing 34 in location 4\n"
      // bar is always 3 large so location 5 is skipped
      "mem: storing 1 in location 6\n"
      "mem: storing 34 in location 7\n"
      "mem: storing 35 in location 8\n"
  );
}
