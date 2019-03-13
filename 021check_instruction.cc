//: Introduce a new transform to perform various checks in instructions before
//: we start running them. It'll be extensible, so that we can add checks for
//: new recipes as we extend 'run' to support them.
//:
//: Doing checking in a separate part complicates things, because the values
//: of variables in memory and the processor (current_recipe_name,
//: current_instruction) aren't available at checking time. If I had a more
//: sophisticated layer system I'd introduce the simpler version first and
//: transform it in a separate layer or set of layers.

:(before "End Checks")
Transform.push_back(check_instruction);  // idempotent

:(code)
void check_instruction(const recipe_ordinal r) {
  trace(101, "transform") << "--- perform checks for recipe " << get(Recipe, r).name << end();
  map<string, vector<type_ordinal> > metadata;
  for (int i = 0;  i < SIZE(get(Recipe, r).steps);  ++i) {
    instruction& inst = get(Recipe, r).steps.at(i);
    if (inst.is_label) continue;
    switch (inst.operation) {
      // Primitive Recipe Checks
      case COPY: {
        if (SIZE(inst.products) > SIZE(inst.ingredients)) {
          raise << maybe(get(Recipe, r).name) << "too many products in '" << to_original_string(inst) << "'\n" << end();
          break;
        }
        for (int i = 0;  i < SIZE(inst.products);  ++i) {
          if (!types_coercible(inst.products.at(i), inst.ingredients.at(i))) {
            raise << maybe(get(Recipe, r).name) << "can't copy '" << inst.ingredients.at(i).original_string << "' to '" << inst.products.at(i).original_string << "'; types don't match\n" << end();
            goto finish_checking_instruction;
          }
        }
        break;
      }
      // End Primitive Recipe Checks
      default: {
        // Defined Recipe Checks
        // End Defined Recipe Checks
      }
    }
    finish_checking_instruction:;
  }
}

void test_copy_checks_reagent_count() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:num, 2:num <- copy 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: too many products in '1:num, 2:num <- copy 34'\n"
  );
}

void test_write_scalar_to_array_disallowed() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:array:num <- copy 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't copy '34' to '1:array:num'; types don't match\n"
  );
}

void test_write_scalar_to_array_disallowed_2() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:num, 2:array:num <- copy 34, 35\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't copy '35' to '2:array:num'; types don't match\n"
  );
}

void test_write_scalar_to_address_disallowed() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:&:num <- copy 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't copy '34' to '1:&:num'; types don't match\n"
  );
}

void test_write_address_to_character_disallowed() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:&:num <- copy 12/unsafe\n"
      "  2:char <- copy 1:&:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't copy '1:&:num' to '2:char'; types don't match\n"
  );
}

void test_write_number_to_character_allowed() {
  run(
      "def main [\n"
      "  1:num <- copy 97\n"
      "  2:char <- copy 1:num\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

:(code)
// types_match with some leniency
bool types_coercible(reagent/*copy*/ to, reagent/*copy*/ from) {
  // Begin types_coercible(reagent to, reagent from)
  if (types_match_sub(to, from)) return true;
  if (is_real_mu_number(from) && is_mu_character(to)) return true;
  // End types_coercible Special-cases
  return false;
}

bool types_match_sub(const reagent& to, const reagent& from) {
  // to sidestep type-checking, use /unsafe in the source.
  // this will be highlighted in red inside vim. just for setting up some tests.
  if (is_unsafe(from)) return true;
  if (is_literal(from)) {
    if (is_mu_array(to)) return false;
    // End Matching Types For Literal(to)
    if (!to.type) return false;
    // allow writing null to any address
    if (is_mu_address(to)) return from.name == "null";
    return size_of(to) == 1;  // literals are always scalars
  }
  return types_strictly_match_sub(to, from);
}
// variant for others to call
bool types_match(reagent/*copy*/ to, reagent/*copy*/ from) {
  // Begin types_match(reagent to, reagent from)
  return types_match_sub(to, from);
}

//: copy arguments for later layers
bool types_strictly_match_sub(const reagent& to, const reagent& from) {
  if (to.type == NULL) return false;  // error
  if (is_literal(from) && to.type->value == Number_type_ordinal) return true;
  // to sidestep type-checking, use /unsafe in the source.
  // this will be highlighted in red inside vim. just for setting up some tests.
  if (is_unsafe(from)) return true;
  // '_' never raises type error
  if (is_dummy(to)) return true;
  if (!to.type) return !from.type;
  return types_strictly_match(to.type, from.type);
}
// variant for others to call
bool types_strictly_match(reagent/*copy*/ to, reagent/*copy*/ from) {
  // Begin types_strictly_match(reagent to, reagent from)
  return types_strictly_match_sub(to, from);
}

bool types_strictly_match(const type_tree* to, const type_tree* from) {
  if (from == to) return true;
  if (!to) return false;
  if (!from) return to->atom && to->value == 0;
  if (from->atom != to->atom) return false;
  if (from->atom) {
    if (from->value == -1) return from->name == to->name;
    return from->value == to->value;
  }
  if (types_strictly_match(to->left, from->left) && types_strictly_match(to->right, from->right))
    return true;
  // fallback: (x) == x
  if (to->right == NULL && types_strictly_match(to->left, from)) return true;
  if (from->right == NULL && types_strictly_match(to, from->left)) return true;
  return false;
}

void test_unknown_type_does_not_match_unknown_type() {
  reagent a("a:foo");
  reagent b("b:bar");
  CHECK(!types_strictly_match(a, b));
}

void test_unknown_type_matches_itself() {
  reagent a("a:foo");
  reagent b("b:foo");
  CHECK(types_strictly_match(a, b));
}

void test_type_abbreviations_match_raw_types() {
  put(Type_abbreviations, "text", new_type_tree("address:array:character"));
  // a has type (address buffer (address array character))
  reagent a("a:address:buffer:text");
  expand_type_abbreviations(a.type);
  // b has type (address buffer address array character)
  reagent b("b:address:buffer:address:array:character");
  CHECK(types_strictly_match(a, b));
  delete Type_abbreviations["text"];
  put(Type_abbreviations, "text", NULL);
}

//: helpers

bool is_unsafe(const reagent& r) {
  return has_property(r, "unsafe");
}

bool is_mu_array(reagent/*copy*/ r) {
  // End Preprocess is_mu_array(reagent r)
  return is_mu_array(r.type);
}
bool is_mu_array(const type_tree* type) {
  if (!type) return false;
  if (is_literal(type)) return false;
  if (type->atom) return false;
  if (!type->left->atom) {
    raise << "invalid type " << to_string(type) << '\n' << end();
    return false;
  }
  return type->left->value == Array_type_ordinal;
}

bool is_mu_boolean(reagent/*copy*/ r) {
  // End Preprocess is_mu_boolean(reagent r)
  if (!r.type) return false;
  if (is_literal(r)) return false;
  if (!r.type->atom) return false;
  return r.type->value == Boolean_type_ordinal;
}

bool is_mu_number(reagent/*copy*/ r) {
  if (is_mu_character(r.type)) return true;  // permit arithmetic on unicode code points
  return is_real_mu_number(r);
}

bool is_real_mu_number(reagent/*copy*/ r) {
  // End Preprocess is_mu_number(reagent r)
  if (!r.type) return false;
  if (!r.type->atom) return false;
  if (is_literal(r)) {
    return r.type->name == "literal-fractional-number"
        || r.type->name == "literal";
  }
  return r.type->value == Number_type_ordinal;
}

bool is_mu_character(reagent/*copy*/ r) {
  // End Preprocess is_mu_character(reagent r)
  return is_mu_character(r.type);
}
bool is_mu_character(const type_tree* type) {
  if (!type) return false;
  if (!type->atom) return false;
  if (is_literal(type)) return false;
  return type->value == Character_type_ordinal;
}
