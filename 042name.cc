//: A big convenience high-level languages provide is the ability to name memory
//: locations. In Mu, a transform called 'transform_names' provides this
//: convenience.

void test_transform_names() {
  run(
      "def main [\n"
      "  x:num <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "name: assign x 2\n"
      "mem: storing 0 in location 2\n"
  );
}

void test_transform_names_fails_on_use_before_define() {
  Hide_errors = true;
  transform(
      "def main [\n"
      "  x:num <- copy y:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: tried to read ingredient 'y' in 'x:num <- copy y:num' but it hasn't been written to yet\n"
  );
}

// todo: detect conditional defines

:(after "End Type Modifying Transforms")
Transform.push_back(transform_names);  // idempotent

:(before "End Globals")
map<recipe_ordinal, map<string, int> > Name;

//: the Name map is a global, so save it before tests and reset it for every
//: test, just to be safe.
:(before "End Globals")
map<recipe_ordinal, map<string, int> > Name_snapshot;
:(before "End save_snapshots")
Name_snapshot = Name;
:(before "End restore_snapshots")
Name = Name_snapshot;

:(code)
void transform_names(const recipe_ordinal r) {
  recipe& caller = get(Recipe, r);
  trace(101, "transform") << "--- transform names for recipe " << caller.name << end();
  bool names_used = false;
  bool numeric_locations_used = false;
  map<string, int>& names = Name[r];
  // record the indices 'used' so far in the map
  int& curr_idx = names[""];
  // reserve indices 0 and 1 for the chaining slot in a later layer.
  // transform_names may get called multiple times in later layers, so
  // curr_idx may already be set.
  if (curr_idx < 2) curr_idx = 2;
  for (int i = 0;  i < SIZE(caller.steps);  ++i) {
    instruction& inst = caller.steps.at(i);
    // End transform_names(inst) Special-cases
    // map names to addresses
    for (int in = 0;  in < SIZE(inst.ingredients);  ++in) {
      reagent& ingredient = inst.ingredients.at(in);
      if (is_disqualified(ingredient, inst, caller.name)) continue;
      if (is_numeric_location(ingredient)) numeric_locations_used = true;
      if (is_named_location(ingredient)) names_used = true;
      if (is_integer(ingredient.name)) continue;
      if (!already_transformed(ingredient, names)) {
        raise << maybe(caller.name) << "tried to read ingredient '" << ingredient.name << "' in '" << to_original_string(inst) << "' but it hasn't been written to yet\n" << end();
        // use-before-set Error
        return;
      }
      int v = lookup_name(ingredient, r);
      if (v >= 0) {
        ingredient.set_value(v);
        // Done Placing Ingredient(ingredient, inst, caller)
      }
      else {
        raise << maybe(caller.name) << "can't find a place to store '" << ingredient.name << "'\n" << end();
        return;
      }
    }
    for (int out = 0;  out < SIZE(inst.products);  ++out) {
      reagent& product = inst.products.at(out);
      if (is_disqualified(product, inst, caller.name)) continue;
      if (is_numeric_location(product)) numeric_locations_used = true;
      if (is_named_location(product)) names_used = true;
      if (is_integer(product.name)) continue;
      if (names.find(product.name) == names.end()) {
        trace(103, "name") << "assign " << product.name << " " << curr_idx << end();
        names[product.name] = curr_idx;
        curr_idx += size_of(product);
      }
      int v = lookup_name(product, r);
      if (v >= 0) {
        product.set_value(v);
        // Done Placing Product(product, inst, caller)
      }
      else {
        raise << maybe(caller.name) << "can't find a place to store '" << product.name << "'\n" << end();
        return;
      }
    }
  }
  if (names_used && numeric_locations_used)
    raise << maybe(caller.name) << "mixing variable names and numeric addresses\n" << end();
}

bool is_disqualified(/*mutable*/ reagent& x, const instruction& inst, const string& recipe_name) {
  if (!x.type) {
    raise << maybe(recipe_name) << "missing type for '" << x.original_string << "' in '" << to_original_string(inst) << "'\n" << end();
    // missing-type Error 1
    return true;
  }
  if (is_raw(x)) return true;
  if (is_literal(x)) return true;
  // End is_disqualified Special-cases
  if (x.initialized) return true;
  return false;
}

bool already_transformed(const reagent& r, const map<string, int>& names) {
  return contains_key(names, r.name);
}

int lookup_name(const reagent& r, const recipe_ordinal default_recipe) {
  return Name[default_recipe][r.name];
}

type_ordinal skip_addresses(type_tree* type) {
  while (type && is_compound_type_starting_with(type, "address"))
    type = type->right;
  if (!type) return -1;  // error handled elsewhere
  if (type->atom) return type->value;
  const type_tree* base_type = type;
  // Update base_type in skip_addresses
  if (base_type->atom)
    return base_type->value;
  assert(base_type->left->atom);
  return base_type->left->value;
}

bool is_compound_type_starting_with(const type_tree* type, const string& expected_name) {
  if (!type) return false;
  if (type->atom) return false;
  if (!type->left->atom) return false;
  return type->left->value == get(Type_ordinal, expected_name);
}

int find_element_offset(const type_ordinal t, const string& name, const string& recipe_name) {
  const type_info& container = get(Type, t);
  for (int i = 0;  i < SIZE(container.elements);  ++i)
    if (container.elements.at(i).name == name) return i;
  raise << maybe(recipe_name) << "unknown element '" << name << "' in container '" << get(Type, t).name << "'\n" << end();
  return -1;
}
int find_element_location(int base_address, const string& name, const type_tree* type, const string& recipe_name) {
  int offset = find_element_offset(get_base_type(type)->value, name, recipe_name);
  if (offset == -1) return offset;
  int result = base_address;
  for (int i = 0; i < offset; ++i)
    result += size_of(element_type(type, i));
  return result;
}

bool is_numeric_location(const reagent& x) {
  if (is_literal(x)) return false;
  if (is_raw(x)) return false;
  if (x.name == "0") return false;  // used for chaining lexical scopes
  return is_integer(x.name);
}

bool is_named_location(const reagent& x) {
  if (is_literal(x)) return false;
  if (is_raw(x)) return false;
  if (is_special_name(x.name)) return false;
  return !is_integer(x.name);
}

// all names here should either be disqualified or also in bind_special_scenario_names
bool is_special_name(const string& s) {
  if (s == "_") return true;
  if (s == "0") return true;
  // End is_special_name Special-cases
  return false;
}

bool is_raw(const reagent& r) {
  return has_property(r, "raw");
}

void test_transform_names_supports_containers() {
  transform(
      "def main [\n"
      "  x:point <- merge 34, 35\n"
      "  y:num <- copy 3\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "name: assign x 2\n"
      // skip location 3 because x occupies two locations
      "name: assign y 4\n"
  );
}

void test_transform_names_supports_static_arrays() {
  transform(
      "def main [\n"
      "  x:@:num:3 <- create-array\n"
      "  y:num <- copy 3\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "name: assign x 2\n"
      // skip locations 3, 4, 5 because x occupies four locations
      "name: assign y 6\n"
  );
}

void test_transform_names_passes_dummy() {
  transform(
      "def main [\n"
      // _ is just a dummy result that never gets consumed
      "  _, x:num <- copy 0, 1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "name: assign x 2\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("name: assign _ 2");
}

//: an escape hatch to suppress name conversion that we'll use later
void test_transform_names_passes_raw() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  x:num/raw <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("name: assign x 2");
  CHECK_TRACE_CONTENTS(
      "error: can't write to location 0 in 'x:num/raw <- copy 0'\n"
  );
}

void test_transform_names_fails_when_mixing_names_and_numeric_locations() {
  Hide_errors = true;
  transform(
      "def main [\n"
      "  x:num <- copy 1:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: mixing variable names and numeric addresses\n"
  );
}

void test_transform_names_fails_when_mixing_names_and_numeric_locations_2() {
  Hide_errors = true;
  transform(
      "def main [\n"
      "  x:num <- copy 1\n"
      "  1:num <- copy x:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: mixing variable names and numeric addresses\n"
  );
}

void test_transform_names_does_not_fail_when_mixing_names_and_raw_locations() {
  transform(
      "def main [\n"
      "  x:num <- copy 1:num/raw\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("error: main: mixing variable names and numeric addresses");
  CHECK_TRACE_COUNT("error", 0);
}

void test_transform_names_does_not_fail_when_mixing_names_and_literals() {
  transform(
      "def main [\n"
      "  x:num <- copy 1\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("error: main: mixing variable names and numeric addresses");
  CHECK_TRACE_COUNT("error", 0);
}

//:: Support element names for containers in 'get' and 'get-location' and 'put'.
//: (get-location is implemented later)

:(before "End update GET offset_value in Check")
else {
  if (!offset.initialized) {
    raise << maybe(get(Recipe, r).name) << "uninitialized offset '" << offset.name << "' in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  offset_value = offset.value;
}

:(code)
void test_transform_names_transforms_container_elements() {
  transform(
      "def main [\n"
      "  p:&:point <- copy null\n"
      "  a:num <- get *p:&:point, y:offset\n"
      "  b:num <- get *p:&:point, x:offset\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "name: element y of type point is at offset 1\n"
      "name: element x of type point is at offset 0\n"
  );
}

:(before "End transform_names(inst) Special-cases")
// replace element names of containers with offsets
if (inst.name == "get" || inst.name == "get-location" || inst.name == "put") {
  //: avoid raising any errors here; later layers will support overloading new
  //: instructions with the same names (static dispatch), which could lead to
  //: spurious errors
  if (SIZE(inst.ingredients) < 2)
    break;  // error raised elsewhere
  if (!is_literal(inst.ingredients.at(1)))
    break;  // error raised elsewhere
  if (inst.ingredients.at(1).name.find_first_not_of("0123456789") != string::npos) {
    // since first non-address in base type must be a container, we don't have to canonize
    type_ordinal base_type = skip_addresses(inst.ingredients.at(0).type);
    if (contains_key(Type, base_type)) {  // otherwise we'll raise an error elsewhere
      inst.ingredients.at(1).set_value(find_element_offset(base_type, inst.ingredients.at(1).name, get(Recipe, r).name));
      trace(103, "name") << "element " << inst.ingredients.at(1).name << " of type " << get(Type, base_type).name << " is at offset " << no_scientific(inst.ingredients.at(1).value) << end();
    }
  }
}

:(code)
void test_missing_type_in_get() {
  Hide_errors = true;
  transform(
      "def main [\n"
      "  get a, x:offset\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: missing type for 'a' in 'get a, x:offset'\n"
  );
}

void test_transform_names_handles_containers() {
  transform(
      "def main [\n"
      "  a:point <- merge 0, 0\n"
      "  b:num <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "name: assign a 2\n"
      "name: assign b 4\n"
  );
}

//:: Support variant names for exclusive containers in 'maybe-convert'.

void test_transform_names_handles_exclusive_containers() {
  run(
      "def main [\n"
      "  12:num <- copy 1\n"
      "  13:num <- copy 35\n"
      "  14:num <- copy 36\n"
      "  20:point, 22:bool <- maybe-convert 12:number-or-point/unsafe, p:variant\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "name: variant p of type number-or-point has tag 1\n"
      "mem: storing 1 in location 22\n"
      "mem: storing 35 in location 20\n"
      "mem: storing 36 in location 21\n"
  );
}

:(before "End transform_names(inst) Special-cases")
// convert variant names of exclusive containers
if (inst.name == "maybe-convert") {
  if (SIZE(inst.ingredients) != 2) {
    raise << maybe(get(Recipe, r).name) << "exactly 2 ingredients expected in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  assert(is_literal(inst.ingredients.at(1)));
  if (inst.ingredients.at(1).name.find_first_not_of("0123456789") != string::npos) {
    // since first non-address in base type must be an exclusive container, we don't have to canonize
    type_ordinal base_type = skip_addresses(inst.ingredients.at(0).type);
    if (contains_key(Type, base_type)) {  // otherwise we'll raise an error elsewhere
      inst.ingredients.at(1).set_value(find_element_offset(base_type, inst.ingredients.at(1).name, get(Recipe, r).name));
      trace(103, "name") << "variant " << inst.ingredients.at(1).name << " of type " << get(Type, base_type).name << " has tag " << no_scientific(inst.ingredients.at(1).value) << end();
    }
  }
}

:(code)
void test_missing_type_in_maybe_convert() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  maybe-convert a, x:variant\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: missing type for 'a' in 'maybe-convert a, x:variant'\n"
  );
}
