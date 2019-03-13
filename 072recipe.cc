//: So far we've been calling a fixed recipe in each instruction, but we'd
//: also like to make the recipe a variable, pass recipes to "higher-order"
//: recipes, return recipes from recipes and so on.

void test_call_literal_recipe() {
  run(
      "def main [\n"
      "  1:num <- call f, 34\n"
      "]\n"
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

:(before "End Mu Types Initialization")
put(Type_ordinal, "recipe-literal", 0);
// 'recipe' variables can store recipe-literal
type_ordinal recipe = put(Type_ordinal, "recipe", Next_type_ordinal++);
get_or_insert(Type, recipe).name = "recipe";

:(after "Deduce Missing Type(x, caller)")
if (!x.type)
  try_initialize_recipe_literal(x, caller);
:(before "Type Check in Type-ingredient-aware check_or_set_types_by_name")
if (!x.type)
  try_initialize_recipe_literal(x, variant);
:(code)
void try_initialize_recipe_literal(reagent& x, const recipe& caller) {
  if (x.type) return;
  if (!contains_key(Recipe_ordinal, x.name)) return;
  if (contains_reagent_with_non_recipe_literal_type(caller, x.name)) return;
  x.type = new type_tree("recipe-literal");
  x.set_value(get(Recipe_ordinal, x.name));
}
bool contains_reagent_with_non_recipe_literal_type(const recipe& caller, const string& name) {
  for (int i = 0;  i < SIZE(caller.steps);  ++i) {
    const instruction& inst = caller.steps.at(i);
    for (int i = 0;  i < SIZE(inst.ingredients);  ++i)
      if (is_matching_non_recipe_literal(inst.ingredients.at(i), name)) return true;
    for (int i = 0;  i < SIZE(inst.products);  ++i)
      if (is_matching_non_recipe_literal(inst.products.at(i), name)) return true;
  }
  return false;
}
bool is_matching_non_recipe_literal(const reagent& x, const string& name) {
  if (x.name != name) return false;
  if (!x.type) return false;
  return !x.type->atom || x.type->name != "recipe-literal";
}

//: It's confusing to use variable names that are also recipe names. Always
//: assume variable types override recipe literals.
void test_error_on_recipe_literal_used_as_a_variable() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  local-scope\n"
      "  a:bool <- equal break 0\n"
      "  break:bool <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: missing type for 'break' in 'a:bool <- equal break, 0'\n"
  );
}

:(before "End Primitive Recipe Declarations")
CALL,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "call", CALL);
:(before "End Primitive Recipe Checks")
case CALL: {
  if (inst.ingredients.empty()) {
    raise << maybe(get(Recipe, r).name) << "'call' requires at least one ingredient (the recipe to call)\n" << end();
    break;
  }
  if (!is_mu_recipe(inst.ingredients.at(0))) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'call' should be a recipe, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case CALL: {
  // Begin Call
  trace(Callstack_depth+1, "trace") << "indirect 'call': incrementing callstack depth to " << Callstack_depth << end();
  ++Callstack_depth;
  assert(Callstack_depth < Max_depth);
  if (!ingredients.at(0).at(0)) {
    raise << maybe(current_recipe_name()) << "tried to call empty recipe in '" << to_string(current_instruction()) << "'" << end();
    break;
  }
  const call& caller_frame = current_call();
  instruction/*copy*/ call_instruction = to_instruction(caller_frame);
  call_instruction.operation = ingredients.at(0).at(0);
  call_instruction.ingredients.erase(call_instruction.ingredients.begin());
  Current_routine->calls.push_front(call(ingredients.at(0).at(0)));
  ingredients.erase(ingredients.begin());  // drop the callee
  finish_call_housekeeping(call_instruction, ingredients);
  // not done with caller
  write_products = false;
  fall_through_to_next_instruction = false;
  break;
}

:(code)
void test_call_variable() {
  run(
      "def main [\n"
      "  {1: (recipe number -> number)} <- copy f\n"
      "  2:num <- call {1: (recipe number -> number)}, 34\n"
      "]\n"
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 2\n"
  );
}

void test_call_literal_recipe_repeatedly() {
  run(
      "def main [\n"
      "  1:num <- call f, 34\n"
      "  1:num <- call f, 35\n"
      "]\n"
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
      "mem: storing 35 in location 1\n"
  );
}

void test_call_shape_shifting_recipe() {
  run(
      "def main [\n"
      "  1:num <- call f, 34\n"
      "]\n"
      "def f x:_elem -> y:_elem [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

void test_call_shape_shifting_recipe_inside_shape_shifting_recipe() {
  run(
      "def main [\n"
      "  1:num <- f 34\n"
      "]\n"
      "def f x:_elem -> y:_elem [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- call g x\n"
      "]\n"
      "def g x:_elem -> y:_elem [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

void test_call_shape_shifting_recipe_repeatedly_inside_shape_shifting_recipe() {
  run(
      "def main [\n"
      "  1:num <- f 34\n"
      "]\n"
      "def f x:_elem -> y:_elem [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- call g x\n"
      "  y <- call g x\n"
      "]\n"
      "def g x:_elem -> y:_elem [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

//:: check types for 'call' instructions

void test_call_check_literal_recipe() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:num <- call f, 34\n"
      "]\n"
      "def f x:point -> y:point [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: ingredient 0 has the wrong type at '1:num <- call f, 34'\n"
      "error: main: product 0 has the wrong type at '1:num <- call f, 34'\n"
  );
}

void test_call_check_variable_recipe() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  {1: (recipe point -> point)} <- copy f\n"
      "  2:num <- call {1: (recipe point -> point)}, 34\n"
      "]\n"
      "def f x:point -> y:point [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: ingredient 0 has the wrong type at '2:num <- call {1: (recipe point -> point)}, 34'\n"
      "error: main: product 0 has the wrong type at '2:num <- call {1: (recipe point -> point)}, 34'\n"
  );
}

:(before "End resolve_ambiguous_call(r, index, inst, caller_recipe) Special-cases")
if (inst.name == "call" && !inst.ingredients.empty() && is_recipe_literal(inst.ingredients.at(0))) {
  resolve_indirect_ambiguous_call(r, index, inst, caller_recipe);
  return;
}
:(code)
bool is_recipe_literal(const reagent& x) {
  return x.type && x.type->atom && x.type->name == "recipe-literal";
}
void resolve_indirect_ambiguous_call(const recipe_ordinal r, int index, instruction& inst, const recipe& caller_recipe) {
  instruction inst2;
  inst2.name = inst.ingredients.at(0).name;
  for (int i = /*skip recipe*/1;  i < SIZE(inst.ingredients);  ++i)
    inst2.ingredients.push_back(inst.ingredients.at(i));
  for (int i = 0;  i < SIZE(inst.products);  ++i)
    inst2.products.push_back(inst.products.at(i));
  resolve_ambiguous_call(r, index, inst2, caller_recipe);
  inst.ingredients.at(0).name = inst2.name;
  inst.ingredients.at(0).set_value(get(Recipe_ordinal, inst2.name));
}

:(after "Transform.push_back(check_instruction)")
Transform.push_back(check_indirect_calls_against_header);  // idempotent
:(code)
void check_indirect_calls_against_header(const recipe_ordinal r) {
  trace(101, "transform") << "--- type-check 'call' instructions inside recipe " << get(Recipe, r).name << end();
  const recipe& caller = get(Recipe, r);
  for (int i = 0;  i < SIZE(caller.steps);  ++i) {
    const instruction& inst = caller.steps.at(i);
    if (!is_indirect_call(inst.operation)) continue;
    if (inst.ingredients.empty()) continue;  // error raised above
    const reagent& callee = inst.ingredients.at(0);
    if (!is_mu_recipe(callee)) continue;  // error raised above
    const recipe callee_header = is_literal(callee) ? get(Recipe, callee.value) : from_reagent(inst.ingredients.at(0));
    if (!callee_header.has_header) continue;
    if (is_indirect_call_with_ingredients(inst.operation)) {
      for (long int i = /*skip callee*/1;  i < min(SIZE(inst.ingredients), SIZE(callee_header.ingredients)+/*skip callee*/1);  ++i) {
        if (!types_coercible(callee_header.ingredients.at(i-/*skip callee*/1), inst.ingredients.at(i)))
          raise << maybe(caller.name) << "ingredient " << i-/*skip callee*/1 << " has the wrong type at '" << to_original_string(inst) << "'\n" << end();
      }
    }
    if (is_indirect_call_with_products(inst.operation)) {
      for (long int i = 0;  i < min(SIZE(inst.products), SIZE(callee_header.products));  ++i) {
        if (is_dummy(inst.products.at(i))) continue;
        if (!types_coercible(callee_header.products.at(i), inst.products.at(i)))
          raise << maybe(caller.name) << "product " << i << " has the wrong type at '" << to_original_string(inst) << "'\n" << end();
      }
    }
  }
}

bool is_indirect_call(const recipe_ordinal r) {
  return is_indirect_call_with_ingredients(r) || is_indirect_call_with_products(r);
}

bool is_indirect_call_with_ingredients(const recipe_ordinal r) {
  if (r == CALL) return true;
  // End is_indirect_call_with_ingredients Special-cases
  return false;
}
bool is_indirect_call_with_products(const recipe_ordinal r) {
  if (r == CALL) return true;
  // End is_indirect_call_with_products Special-cases
  return false;
}

recipe from_reagent(const reagent& r) {
  assert(r.type);
  recipe result_header;  // will contain only ingredients and products, nothing else
  result_header.has_header = true;
  // Begin Reagent->Recipe(r, recipe_header)
  if (r.type->atom) {
    assert(r.type->name == "recipe");
    return result_header;
  }
  const type_tree* root_type = r.type->atom ? r.type : r.type->left;
  assert(root_type->atom);
  assert(root_type->name == "recipe");
  const type_tree* curr = r.type->right;
  for (/*nada*/;  curr && !curr->atom;  curr = curr->right) {
    if (curr->left->atom && curr->left->name == "->") {
      curr = curr->right;  // skip delimiter
      goto read_products;
    }
    result_header.ingredients.push_back(next_recipe_reagent(curr->left));
  }
  if (curr) {
    assert(curr->atom);
    result_header.ingredients.push_back(next_recipe_reagent(curr));
    return result_header;  // no products
  }
  read_products:
  for (/*nada*/;  curr && !curr->atom;  curr = curr->right)
    result_header.products.push_back(next_recipe_reagent(curr->left));
  if (curr) {
    assert(curr->atom);
    result_header.products.push_back(next_recipe_reagent(curr));
  }
  return result_header;
}

:(before "End Unit Tests")
void test_from_reagent_atomic() {
  reagent a("{f: recipe}");
  recipe r_header = from_reagent(a);
  CHECK(r_header.ingredients.empty());
  CHECK(r_header.products.empty());
}
void test_from_reagent_non_atomic() {
  reagent a("{f: (recipe number -> number)}");
  recipe r_header = from_reagent(a);
  CHECK_EQ(SIZE(r_header.ingredients), 1);
  CHECK_EQ(SIZE(r_header.products), 1);
}
void test_from_reagent_reads_ingredient_at_end() {
  reagent a("{f: (recipe number number)}");
  recipe r_header = from_reagent(a);
  CHECK_EQ(SIZE(r_header.ingredients), 2);
  CHECK(r_header.products.empty());
}
void test_from_reagent_reads_sole_ingredient_at_end() {
  reagent a("{f: (recipe number)}");
  recipe r_header = from_reagent(a);
  CHECK_EQ(SIZE(r_header.ingredients), 1);
  CHECK(r_header.products.empty());
}

:(code)
reagent next_recipe_reagent(const type_tree* curr) {
  if (!curr->left) return reagent("recipe:"+curr->name);
  return reagent(new type_tree(*curr));
}

bool is_mu_recipe(const reagent& r) {
  if (!r.type) return false;
  if (r.type->atom) {
    // End is_mu_recipe Atom Cases(r)
    return r.type->name == "recipe-literal";
  }
  return r.type->left->atom && r.type->left->name == "recipe";
}

void test_copy_typecheck_recipe_variable() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  3:num <- copy 34\n"
      "  {1: (recipe number -> number)} <- copy f\n"  // store literal in a matching variable
      "  {2: (recipe boolean -> boolean)} <- copy {1: (recipe number -> number)}\n"  // mismatch between recipe variables
      "]\n"
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't copy '{1: (recipe number -> number)}' to '{2: (recipe boolean -> boolean)}'; types don't match\n"
  );
}

void test_copy_typecheck_recipe_variable_2() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  {1: (recipe number -> number)} <- copy f\n"  // mismatch with a recipe literal
      "]\n"
      "def f x:bool -> y:bool [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't copy 'f' to '{1: (recipe number -> number)}'; types don't match\n"
  );
}

:(before "End Matching Types For Literal(to)")
if (is_mu_recipe(to)) {
  if (!contains_key(Recipe, from.value)) {
    raise << "trying to store recipe " << from.name << " into " << to_string(to) << " but there's no such recipe\n" << end();
    return false;
  }
  const recipe& rrhs = get(Recipe, from.value);
  const recipe& rlhs = from_reagent(to);
  for (long int i = 0;  i < min(SIZE(rlhs.ingredients), SIZE(rrhs.ingredients));  ++i) {
    if (!types_match(rlhs.ingredients.at(i), rrhs.ingredients.at(i)))
      return false;
  }
  for (long int i = 0;  i < min(SIZE(rlhs.products), SIZE(rrhs.products));  ++i) {
    if (!types_match(rlhs.products.at(i), rrhs.products.at(i)))
      return false;
  }
  return true;
}

:(code)
void test_call_variable_compound_ingredient() {
  run(
      "def main [\n"
      "  {1: (recipe (address number) -> number)} <- copy f\n"
      "  2:&:num <- copy null\n"
      "  3:num <- call {1: (recipe (address number) -> number)}, 2:&:num\n"
      "]\n"
      "def f x:&:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- deaddress x\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

//: make sure we don't accidentally break on a recipe literal
void test_jump_forbidden_on_recipe_literals() {
  Hide_errors = true;
  run(
      "def foo [\n"
      "  local-scope\n"
      "]\n"
      "def main [\n"
      "  local-scope\n"
      "  {\n"
      "    break-if foo\n"
      "  }\n"
      "]\n"
  );
  // error should be as if foo is not a recipe
  CHECK_TRACE_CONTENTS(
      "error: main: missing type for 'foo' in 'break-if foo'\n"
  );
}

:(before "End JUMP_IF Checks")
check_for_recipe_literals(inst, get(Recipe, r));
:(before "End JUMP_UNLESS Checks")
check_for_recipe_literals(inst, get(Recipe, r));
:(code)
void check_for_recipe_literals(const instruction& inst, const recipe& caller) {
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (is_mu_recipe(inst.ingredients.at(i))) {
      raise << maybe(caller.name) << "missing type for '" << inst.ingredients.at(i).original_string << "' in '" << to_original_string(inst) << "'\n" << end();
      if (is_present_in_ingredients(caller, inst.ingredients.at(i).name))
        raise << "  did you forget 'load-ingredients'?\n" << end();
    }
  }
}

void test_load_ingredients_missing_error_3() {
  Hide_errors = true;
  run(
      "def foo {f: (recipe num -> num)} [\n"
      "  local-scope\n"
      "  b:num <- call f, 1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: foo: missing type for 'f' in 'b:num <- call f, 1'\n"
      "error:   did you forget 'load-ingredients'?\n"
  );
}

:(before "End Mu Types Initialization")
put(Type_abbreviations, "function", new_type_tree("recipe"));
put(Type_abbreviations, "fn", new_type_tree("recipe"));

//: copying functions to variables

:(code)
void test_copy_recipe_to_variable() {
  run(
      "def main [\n"
      "  {1: (fn number -> number)} <- copy f\n"
      "  2:num <- call {1: (function number -> number)}, 34\n"
      "]\n"
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 2\n"
  );
}

void test_copy_overloaded_recipe_to_variable() {
  run(
      "def main [\n"
      "  local-scope\n"
      "  {x: (fn num -> num)} <- copy f\n"
      "  1:num/raw <- call x, 34\n"
      "]\n"
      // variant f
      "def f x:bool -> y:bool [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
      // variant f_2
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  // x contains f_2
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

:(before "End resolve_ambiguous_call(r, index, inst, caller_recipe) Special-cases")
if (inst.name == "copy") {
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_recipe_literal(inst.ingredients.at(i))) continue;
    if (non_ghost_size(get_or_insert(Recipe_variants, inst.ingredients.at(i).name)) < 1) continue;
    // potentially overloaded recipe
    string new_name = resolve_ambiguous_call(inst.ingredients.at(i).name, inst.products.at(i), r, index, caller_recipe);
    if (new_name == "") continue;
    inst.ingredients.at(i).name = new_name;
    inst.ingredients.at(i).value = get(Recipe_ordinal, new_name);
  }
  return;
}
:(code)
string resolve_ambiguous_call(const string& recipe_name, const reagent& call_types, const recipe_ordinal r, int index, const recipe& caller_recipe) {
  instruction inst;
  inst.name = recipe_name;
  if (!is_mu_recipe(call_types)) return "";  // error raised elsewhere
  if (is_recipe_literal(call_types)) return "";  // error raised elsewhere
  construct_fake_call(call_types, inst);
  resolve_ambiguous_call(r, index, inst, caller_recipe);
  return inst.name;
}
void construct_fake_call(const reagent& recipe_var, instruction& out) {
  assert(recipe_var.type->left->name == "recipe");
  type_tree* stem = NULL;
  for (stem = recipe_var.type->right;  stem && stem->left->name != "->";  stem = stem->right)
    out.ingredients.push_back(copy(stem->left));
  if (stem == NULL) return;
  for (/*skip '->'*/stem = stem->right;  stem;  stem = stem->right)
    out.products.push_back(copy(stem->left));
}

void test_copy_shape_shifting_recipe_to_variable() {
  run(
      "def main [\n"
      "  local-scope\n"
      "  {x: (fn num -> num)} <- copy f\n"
      "  1:num/raw <- call x, 34\n"
      "]\n"
      "def f x:_elem -> y:_elem [\n"
      "  local-scope\n"
      "  load-inputs\n"
      "  y <- copy x\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

//: passing function literals to (higher-order) functions

void test_pass_overloaded_recipe_literal_to_ingredient() {
  run(
      // like test_copy_overloaded_recipe_to_variable, except we bind 'x' in
      // the course of a 'call' rather than 'copy'
      "def main [\n"
      "  1:num <- g f\n"
      "]\n"
      "def g {x: (fn num -> num)} -> result:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  result <- call x, 34\n"
      "]\n"
      // variant f
      "def f x:bool -> y:bool [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
      // variant f_2
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  // x contains f_2
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

:(after "End resolve_ambiguous_call(r, index, inst, caller_recipe) Special-cases")
for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
  if (!is_mu_recipe(inst.ingredients.at(i))) continue;
  if (non_ghost_size(get_or_insert(Recipe_variants, inst.ingredients.at(i).name)) < 1) continue;
  if (get(Recipe_ordinal, inst.name) < MAX_PRIMITIVE_RECIPES) continue;
  if (non_ghost_size(get_or_insert(Recipe_variants, inst.name)) > 1) {
    raise << maybe(caller_recipe.name) << "sorry, we're not yet smart enough to simultaneously guess which overloads you want for '" << inst.name << "' and '" << inst.ingredients.at(i).name << "'\n" << end();
    return;
  }
  const recipe& callee = get(Recipe, get(Recipe_ordinal, inst.name));
  if (!callee.has_header) {
    raise << maybe(caller_recipe.name) << "sorry, we're not yet smart enough to guess which variant of '" << inst.ingredients.at(i).name << "' you want, when the caller '" << inst.name << "' doesn't have a header\n" << end();
    return;
  }
  string new_name = resolve_ambiguous_call(inst.ingredients.at(i).name, callee.ingredients.at(i), r, index, caller_recipe);
  if (new_name != "") {
    inst.ingredients.at(i).name = new_name;
    inst.ingredients.at(i).value = get(Recipe_ordinal, new_name);
  }
}

:(code)
void test_return_overloaded_recipe_literal_to_caller() {
  run(
      "def main [\n"
      "  local-scope\n"
      "  {x: (fn num -> num)} <- g\n"
      "  1:num/raw <- call x, 34\n"
      "]\n"
      "def g -> {x: (fn num -> num)} [\n"
      "  local-scope\n"
      "  return f\n"
      "]\n"
      // variant f
      "def f x:bool -> y:bool [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
      // variant f_2
      "def f x:num -> y:num [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  y <- copy x\n"
      "]\n"
  );
  // x contains f_2
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

:(before "End resolve_ambiguous_call(r, index, inst, caller_recipe) Special-cases")
if (inst.name == "return" || inst.name == "reply") {
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_recipe_literal(inst.ingredients.at(i))) continue;
    if (non_ghost_size(get_or_insert(Recipe_variants, inst.ingredients.at(i).name)) < 1) continue;
    // potentially overloaded recipe
    if (!caller_recipe.has_header) {
      raise << maybe(caller_recipe.name) << "sorry, we're not yet smart enough to guess which variant of '" << inst.ingredients.at(i).name << "' you want, without a recipe header\n" << end();
      return;
    }
    string new_name = resolve_ambiguous_call(inst.ingredients.at(i).name, caller_recipe.products.at(i), r, index, caller_recipe);
    if (new_name == "") continue;
    inst.ingredients.at(i).name = new_name;
    inst.ingredients.at(i).value = get(Recipe_ordinal, new_name);
  }
  return;
}
