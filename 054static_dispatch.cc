//: Transform to maintain multiple variants of a recipe depending on the
//: number and types of the ingredients and products. Allows us to use nice
//: names like 'print' or 'length' in many mutually extensible ways.

:(scenario static_dispatch)
def main [
  7:num/raw <- test 3
]
def test a:num -> z:num [
  z <- copy 1
]
def test a:num, b:num -> z:num [
  z <- copy 2
]
+mem: storing 1 in location 7

//: When loading recipes, accumulate variants if headers don't collide, and
//: flag an error if headers collide.

:(before "End Globals")
map<string, vector<recipe_ordinal> > Recipe_variants;
:(before "End One-time Setup")
put(Recipe_variants, "main", vector<recipe_ordinal>());  // since we manually added main to Recipe_ordinal

:(before "End Globals")
map<string, vector<recipe_ordinal> > Recipe_variants_snapshot;
:(before "End save_snapshots")
Recipe_variants_snapshot = Recipe_variants;
:(before "End restore_snapshots")
Recipe_variants = Recipe_variants_snapshot;

:(before "End Load Recipe Header(result)")
// there can only ever be one variant for main
if (result.name != "main" && contains_key(Recipe_ordinal, result.name)) {
  const recipe_ordinal r = get(Recipe_ordinal, result.name);
  if (!contains_key(Recipe, r) || get(Recipe, r).has_header) {
    string new_name = matching_variant_name(result);
    if (new_name.empty()) {
      // variant doesn't already exist
      new_name = next_unused_recipe_name(result.name);
      put(Recipe_ordinal, new_name, Next_recipe_ordinal++);
      get_or_insert(Recipe_variants, result.name).push_back(get(Recipe_ordinal, new_name));
    }
    trace("load") << "switching " << result.name << " to " << new_name << end();
    result.name = new_name;
    result.is_autogenerated = true;
  }
}
else {
  // save first variant
  put(Recipe_ordinal, result.name, Next_recipe_ordinal++);
  get_or_insert(Recipe_variants, result.name).push_back(get(Recipe_ordinal, result.name));
}

:(code)
string matching_variant_name(const recipe& rr) {
  const vector<recipe_ordinal>& variants = get_or_insert(Recipe_variants, rr.name);
  for (int i = 0;  i < SIZE(variants);  ++i) {
    if (!contains_key(Recipe, variants.at(i))) continue;
    const recipe& candidate = get(Recipe, variants.at(i));
    if (!all_reagents_match(rr, candidate)) continue;
    return candidate.name;
  }
  return "";
}

bool all_reagents_match(const recipe& r1, const recipe& r2) {
  if (SIZE(r1.ingredients) != SIZE(r2.ingredients)) return false;
  if (SIZE(r1.products) != SIZE(r2.products)) return false;
  for (int i = 0;  i < SIZE(r1.ingredients);  ++i) {
    expand_type_abbreviations(r1.ingredients.at(i).type);
    expand_type_abbreviations(r2.ingredients.at(i).type);
    if (!deeply_equal_type_names(r1.ingredients.at(i), r2.ingredients.at(i)))
      return false;
  }
  for (int i = 0;  i < SIZE(r1.products);  ++i) {
    expand_type_abbreviations(r1.products.at(i).type);
    expand_type_abbreviations(r2.products.at(i).type);
    if (!deeply_equal_type_names(r1.products.at(i), r2.products.at(i)))
      return false;
  }
  return true;
}

:(before "End Globals")
set<string> Literal_type_names;
:(before "End One-time Setup")
Literal_type_names.insert("number");
Literal_type_names.insert("character");
:(code)
bool deeply_equal_type_names(const reagent& a, const reagent& b) {
  return deeply_equal_type_names(a.type, b.type);
}
bool deeply_equal_type_names(const type_tree* a, const type_tree* b) {
  if (!a) return !b;
  if (!b) return !a;
  if (a->atom != b->atom) return false;
  if (a->atom) {
    if (a->name == "literal" && b->name == "literal")
      return true;
    if (a->name == "literal")
      return Literal_type_names.find(b->name) != Literal_type_names.end();
    if (b->name == "literal")
      return Literal_type_names.find(a->name) != Literal_type_names.end();
    return a->name == b->name;
  }
  return deeply_equal_type_names(a->left, b->left)
      && deeply_equal_type_names(a->right, b->right);
}

string next_unused_recipe_name(const string& recipe_name) {
  for (int i = 2;  /*forever*/;  ++i) {
    ostringstream out;
    out << recipe_name << '_' << i;
    if (!contains_key(Recipe_ordinal, out.str()))
      return out.str();
  }
}

//: Once all the recipes are loaded, transform their bodies to replace each
//: call with the most suitable variant.

:(scenario static_dispatch_picks_most_similar_variant)
def main [
  7:num/raw <- test 3, 4, 5
]
def test a:num -> z:num [
  z <- copy 1
]
def test a:num, b:num -> z:num [
  z <- copy 2
]
+mem: storing 2 in location 7

//: support recipe headers in a previous transform to fill in missing types
:(before "End check_or_set_invalid_types")
for (int i = 0;  i < SIZE(caller.ingredients);  ++i)
  check_or_set_invalid_types(caller.ingredients.at(i).type, maybe(caller.name), "recipe header ingredient");
for (int i = 0;  i < SIZE(caller.products);  ++i)
  check_or_set_invalid_types(caller.products.at(i).type, maybe(caller.name), "recipe header product");

//: save original name of recipes before renaming them
:(before "End recipe Fields")
string original_name;
//: original name is only set during load
:(before "End Load Recipe Name")
result.original_name = result.name;

//: after filling in all missing types (because we'll be introducing 'blank' types in this transform in a later layer, for shape-shifting recipes)
:(after "Transform.push_back(transform_names)")
Transform.push_back(resolve_ambiguous_calls);  // idempotent

//: In a later layer we'll introduce recursion in resolve_ambiguous_calls, by
//: having it generate code for shape-shifting recipes and then transform such
//: code. This data structure will help error messages be more useful.
//:
//: We're punning the 'call' data structure just because it has slots for
//: calling recipe and calling instruction.
:(before "End Globals")
list<call> Resolve_stack;

:(code)
void resolve_ambiguous_calls(const recipe_ordinal r) {
  recipe& caller_recipe = get(Recipe, r);
  trace(9991, "transform") << "--- resolve ambiguous calls for recipe " << caller_recipe.name << end();
  for (int index = 0;  index < SIZE(caller_recipe.steps);  ++index) {
    instruction& inst = caller_recipe.steps.at(index);
    if (inst.is_label) continue;
    if (non_ghost_size(get_or_insert(Recipe_variants, inst.name)) == 0) continue;
    trace(9992, "transform") << "instruction " << to_original_string(inst) << end();
    Resolve_stack.push_front(call(r));
    Resolve_stack.front().running_step_index = index;
    string new_name = best_variant(inst, caller_recipe);
    if (!new_name.empty())
      inst.name = new_name;
    assert(Resolve_stack.front().running_recipe == r);
    assert(Resolve_stack.front().running_step_index == index);
    Resolve_stack.pop_front();
  }
}

string best_variant(instruction& inst, const recipe& caller_recipe) {
  const vector<recipe_ordinal>& variants = get(Recipe_variants, inst.name);
  vector<recipe_ordinal> candidates;

  // Static Dispatch Phase 1
//?   cerr << inst.name << " phase 1\n";
  candidates = strictly_matching_variants(inst, variants);
  if (!candidates.empty()) return best_variant(inst, candidates).name;

  // Static Dispatch Phase 2
//?   cerr << inst.name << " phase 2\n";
  candidates = strictly_matching_variants_except_literal_against_address_or_boolean(inst, variants);
  if (!candidates.empty()) return best_variant(inst, candidates).name;

//?   cerr << inst.name << " phase 3\n";
  // Static Dispatch Phase 3
  //: (shape-shifting recipes in a later layer)
  // End Static Dispatch Phase 3

  // Static Dispatch Phase 4
//?   cerr << inst.name << " phase 4\n";
  candidates = matching_variants(inst, variants);
  if (!candidates.empty()) return best_variant(inst, candidates).name;

  // error messages
  if (!is_primitive(get(Recipe_ordinal, inst.name))) {  // we currently don't check types for primitive variants
    if (SIZE(variants) == 1) {
      raise << maybe(caller_recipe.name) << "types don't match in call for '" << to_original_string(inst) << "'\n" << end();
      raise << "  which tries to call '" << original_header_label(get(Recipe, variants.at(0))) << "'\n" << end();
    }
    else {
      raise << maybe(caller_recipe.name) << "failed to find a matching call for '" << to_original_string(inst) << "'\n" << end();
      raise << "  available variants are:\n" << end();
      for (int i = 0;  i < SIZE(variants);  ++i)
        raise << "    " << original_header_label(get(Recipe, variants.at(i))) << '\n' << end();
    }
    for (list<call>::iterator p = /*skip*/++Resolve_stack.begin();  p != Resolve_stack.end();  ++p) {
      const recipe& specializer_recipe = get(Recipe, p->running_recipe);
      const instruction& specializer_inst = specializer_recipe.steps.at(p->running_step_index);
      if (specializer_recipe.name != "interactive")
        raise << "  (from '" << to_original_string(specializer_inst) << "' in " << specializer_recipe.name << ")\n" << end();
      else
        raise << "  (from '" << to_original_string(specializer_inst) << "')\n" << end();
      // One special-case to help with the rewrite_stash transform. (cross-layer)
      if (specializer_inst.products.at(0).name.find("stash_") == 0) {
        instruction stash_inst;
        if (next_stash(*p, &stash_inst)) {
          if (specializer_recipe.name != "interactive")
            raise << "  (part of '" << to_original_string(stash_inst) << "' in " << specializer_recipe.name << ")\n" << end();
          else
            raise << "  (part of '" << to_original_string(stash_inst) << "')\n" << end();
        }
      }
    }
  }
  return "";
}

// phase 1
vector<recipe_ordinal> strictly_matching_variants(const instruction& inst, const vector<recipe_ordinal>& variants) {
  vector<recipe_ordinal> result;
  for (int i = 0;  i < SIZE(variants);  ++i) {
    if (variants.at(i) == -1) continue;
    trace(9992, "transform") << "checking variant (strict) " << i << ": " << header_label(variants.at(i)) << end();
    if (all_header_reagents_strictly_match(inst, get(Recipe, variants.at(i))))
      result.push_back(variants.at(i));
  }
  return result;
}

bool all_header_reagents_strictly_match(const instruction& inst, const recipe& variant) {
  for (int i = 0;  i < min(SIZE(inst.ingredients), SIZE(variant.ingredients));  ++i) {
    if (!types_strictly_match(variant.ingredients.at(i), inst.ingredients.at(i))) {
      trace(9993, "transform") << "strict match failed: ingredient " << i << end();
      return false;
    }
  }
  for (int i = 0;  i < min(SIZE(inst.products), SIZE(variant.products));  ++i) {
    if (is_dummy(inst.products.at(i))) continue;
    if (!types_strictly_match(variant.products.at(i), inst.products.at(i))) {
      trace(9993, "transform") << "strict match failed: product " << i << end();
      return false;
    }
  }
  return true;
}

// phase 2
vector<recipe_ordinal> strictly_matching_variants_except_literal_against_address_or_boolean(const instruction& inst, const vector<recipe_ordinal>& variants) {
  vector<recipe_ordinal> result;
  for (int i = 0;  i < SIZE(variants);  ++i) {
    if (variants.at(i) == -1) continue;
    trace(9992, "transform") << "checking variant (strict except literal-against-boolean) " << i << ": " << header_label(variants.at(i)) << end();
    if (all_header_reagents_strictly_match_except_literal_against_address_or_boolean(inst, get(Recipe, variants.at(i))))
      result.push_back(variants.at(i));
  }
  return result;
}

bool all_header_reagents_strictly_match_except_literal_against_address_or_boolean(const instruction& inst, const recipe& variant) {
  for (int i = 0;  i < min(SIZE(inst.ingredients), SIZE(variant.ingredients));  ++i) {
    if (!types_strictly_match_except_literal_against_address_or_boolean(variant.ingredients.at(i), inst.ingredients.at(i))) {
      trace(9993, "transform") << "match failed: ingredient " << i << end();
      return false;
    }
  }
  for (int i = 0;  i < min(SIZE(variant.products), SIZE(inst.products));  ++i) {
    if (is_dummy(inst.products.at(i))) continue;
    if (!types_strictly_match_except_literal_against_address_or_boolean(variant.products.at(i), inst.products.at(i))) {
      trace(9993, "transform") << "match failed: product " << i << end();
      return false;
    }
  }
  return true;
}

bool types_strictly_match_except_literal_against_address_or_boolean(const reagent& to, const reagent& from) {
  if (is_literal(from) && is_mu_boolean(to))
    return from.name == "0" || from.name == "1";
  // Match Literal Zero Against Address {
  if (is_literal(from) && is_mu_address(to))
    return from.name == "0";
  // }
  return types_strictly_match(to, from);
}

// phase 4
vector<recipe_ordinal> matching_variants(const instruction& inst, const vector<recipe_ordinal>& variants) {
  vector<recipe_ordinal> result;
  for (int i = 0;  i < SIZE(variants);  ++i) {
    if (variants.at(i) == -1) continue;
    trace(9992, "transform") << "checking variant " << i << ": " << header_label(variants.at(i)) << end();
    if (all_header_reagents_match(inst, get(Recipe, variants.at(i))))
      result.push_back(variants.at(i));
  }
  return result;
}

bool all_header_reagents_match(const instruction& inst, const recipe& variant) {
  for (int i = 0;  i < min(SIZE(inst.ingredients), SIZE(variant.ingredients));  ++i) {
    if (!types_match(variant.ingredients.at(i), inst.ingredients.at(i))) {
      trace(9993, "transform") << "match failed: ingredient " << i << end();
      return false;
    }
  }
  for (int i = 0;  i < min(SIZE(variant.products), SIZE(inst.products));  ++i) {
    if (is_dummy(inst.products.at(i))) continue;
    if (!types_match(variant.products.at(i), inst.products.at(i))) {
      trace(9993, "transform") << "match failed: product " << i << end();
      return false;
    }
  }
  return true;
}

// tie-breaker for each phase
const recipe& best_variant(const instruction& inst, vector<recipe_ordinal>& candidates) {
  assert(!candidates.empty());
  if (SIZE(candidates) == 1) return get(Recipe, candidates.at(0));
  int min_score = 999;
  int min_index = 0;
  for (int i = 0;  i < SIZE(candidates);  ++i) {
    const recipe& candidate = get(Recipe, candidates.at(i));
    // prefer functions without extra or missing ingredients or products
    int score = abs(SIZE(candidate.products)-SIZE(inst.products))
                          + abs(SIZE(candidate.ingredients)-SIZE(inst.ingredients));
    // prefer functions with non-address ingredients or products
    for (int j = 0;  j < SIZE(candidate.ingredients);  ++j) {
      if (is_mu_address(candidate.ingredients.at(j)))
        ++score;
    }
    for (int j = 0;  j < SIZE(candidate.products);  ++j) {
      if (is_mu_address(candidate.products.at(j)))
        ++score;
    }
    assert(score < 999);
    if (score < min_score) {
      min_score = score;
      min_index = i;
    }
  }
  return get(Recipe, candidates.at(min_index));
}

int non_ghost_size(vector<recipe_ordinal>& variants) {
  int result = 0;
  for (int i = 0;  i < SIZE(variants);  ++i)
    if (variants.at(i) != -1) ++result;
  return result;
}

bool next_stash(const call& c, instruction* stash_inst) {
  const recipe& specializer_recipe = get(Recipe, c.running_recipe);
  int index = c.running_step_index;
  for (++index;  index < SIZE(specializer_recipe.steps);  ++index) {
    const instruction& inst = specializer_recipe.steps.at(index);
    if (inst.name == "stash") {
      *stash_inst = inst;
      return true;
    }
  }
  return false;
}

:(scenario static_dispatch_disabled_in_recipe_without_variants)
def main [
  1:num <- test 3
]
def test [
  2:num <- next-ingredient  # ensure no header
  return 34
]
+mem: storing 34 in location 1

:(scenario static_dispatch_disabled_on_headerless_definition)
% Hide_errors = true;
def test a:num -> z:num [
  z <- copy 1
]
def test [
  return 34
]
+error: redefining recipe test

:(scenario static_dispatch_disabled_on_headerless_definition_2)
% Hide_errors = true;
def test [
  return 34
]
def test a:num -> z:num [
  z <- copy 1
]
+error: redefining recipe test

:(scenario static_dispatch_on_primitive_names)
def main [
  1:num <- copy 34
  2:num <- copy 34
  3:bool <- equal 1:num, 2:num
  4:bool <- copy 0/false
  5:bool <- copy 0/false
  6:bool <- equal 4:bool, 5:bool
]
# temporarily hardcode number equality to always fail
def equal x:num, y:num -> z:bool [
  local-scope
  load-ingredients
  z <- copy 0/false
]
# comparing numbers used overload
+mem: storing 0 in location 3
# comparing booleans continues to use primitive
+mem: storing 1 in location 6

:(scenario static_dispatch_works_with_dummy_results_for_containers)
def main [
  _ <- test 3, 4
]
def test a:num -> z:point [
  local-scope
  load-ingredients
  z <- merge a, 0
]
def test a:num, b:num -> z:point [
  local-scope
  load-ingredients
  z <- merge a, b
]
$error: 0

:(scenario static_dispatch_works_with_compound_type_containing_container_defined_after_first_use)
def main [
  x:&:foo <- new foo:type
  test x
]
container foo [
  x:num
]
def test a:&:foo -> z:num [
  local-scope
  load-ingredients
  z:num <- get *a, x:offset
]
$error: 0

:(scenario static_dispatch_works_with_compound_type_containing_container_defined_after_second_use)
def main [
  x:&:foo <- new foo:type
  test x
]
def test a:&:foo -> z:num [
  local-scope
  load-ingredients
  z:num <- get *a, x:offset
]
container foo [
  x:num
]
$error: 0

:(scenario static_dispatch_prefers_literals_to_be_numbers_rather_than_addresses)
def main [
  1:num <- foo 0
]
def foo x:&:num -> y:num [
  return 34
]
def foo x:num -> y:num [
  return 35
]
+mem: storing 35 in location 1

:(scenario static_dispatch_prefers_literals_to_be_numbers_rather_than_addresses_2)
def main [
  1:num <- foo 0 0
]
# Both variants need to bind 0 to address in first ingredient.
# We still want to prefer the variant with a number rather than address for
# _subsequent_ ingredients.
def foo x:&:num y:&:num -> z:num [  # put the bad match before the good one
  return 34
]
def foo x:&:num y:num -> z:num [
  return 35
]
+mem: storing 35 in location 1

:(scenario static_dispatch_on_non_literal_character_ignores_variant_with_numbers)
% Hide_errors = true;
def main [
  local-scope
  x:char <- copy 10/newline
  1:num/raw <- foo x
]
def foo x:num -> y:num [
  load-ingredients
  return 34
]
+error: main: ingredient 0 has the wrong type at '1:num/raw <- foo x'
-mem: storing 34 in location 1

:(scenario static_dispatch_dispatches_literal_to_boolean_before_character)
def main [
  1:num/raw <- foo 0  # valid literal for boolean
]
def foo x:char -> y:num [
  local-scope
  load-ingredients
  return 34
]
def foo x:bool -> y:num [
  local-scope
  load-ingredients
  return 35
]
# boolean variant is preferred
+mem: storing 35 in location 1

:(scenario static_dispatch_dispatches_literal_to_character_when_out_of_boolean_range)
def main [
  1:num/raw <- foo 97  # not a valid literal for boolean
]
def foo x:char -> y:num [
  local-scope
  load-ingredients
  return 34
]
def foo x:bool -> y:num [
  local-scope
  load-ingredients
  return 35
]
# character variant is preferred
+mem: storing 34 in location 1

:(scenario static_dispatch_dispatches_literal_to_number_if_at_all_possible)
def main [
  1:num/raw <- foo 97
]
def foo x:char -> y:num [
  local-scope
  load-ingredients
  return 34
]
def foo x:num -> y:num [
  local-scope
  load-ingredients
  return 35
]
# number variant is preferred
+mem: storing 35 in location 1

:(replace{} "string header_label(const recipe_ordinal r)")
string header_label(const recipe_ordinal r) {
  return header_label(get(Recipe, r));
}
:(code)
string header_label(const recipe& caller) {
  ostringstream out;
  out << "recipe " << caller.name;
  for (int i = 0;  i < SIZE(caller.ingredients);  ++i)
    out << ' ' << to_string(caller.ingredients.at(i));
  if (!caller.products.empty()) out << " ->";
  for (int i = 0;  i < SIZE(caller.products);  ++i)
    out << ' ' << to_string(caller.products.at(i));
  return out.str();
}

string original_header_label(const recipe& caller) {
  ostringstream out;
  out << "recipe " << caller.original_name;
  for (int i = 0;  i < SIZE(caller.ingredients);  ++i)
    out << ' ' << caller.ingredients.at(i).original_string;
  if (!caller.products.empty()) out << " ->";
  for (int i = 0;  i < SIZE(caller.products);  ++i)
    out << ' ' << caller.products.at(i).original_string;
  return out.str();
}

:(scenario reload_variant_retains_other_variants)
def main [
  1:num <- copy 34
  2:num <- foo 1:num
]
def foo x:num -> y:num [
  local-scope
  load-ingredients
  return 34
]
def foo x:&:num -> y:num [
  local-scope
  load-ingredients
  return 35
]
def! foo x:&:num -> y:num [
  local-scope
  load-ingredients
  return 36
]
+mem: storing 34 in location 2
$error: 0

:(scenario dispatch_errors_come_after_unknown_name_errors)
% Hide_errors = true;
def main [
  y:num <- foo x
]
def foo a:num -> b:num [
  local-scope
  load-ingredients
  return 34
]
def foo a:bool -> b:num [
  local-scope
  load-ingredients
  return 35
]
+error: main: missing type for 'x' in 'y:num <- foo x'
+error: main: failed to find a matching call for 'y:num <- foo x'

:(scenario override_methods_with_type_abbreviations)
def main [
  local-scope
  s:text <- new [abc]
  1:num/raw <- foo s
]
def foo a:address:array:character -> result:number [
  return 34
]
# identical to previous variant once you take type abbreviations into account
def! foo a:text -> result:num [
  return 35
]
+mem: storing 35 in location 1

:(scenario ignore_static_dispatch_in_type_errors_without_overloading)
% Hide_errors = true;
def main [
  local-scope
  x:&:num <- copy 0
  foo x
]
def foo x:&:char [
  local-scope
  load-ingredients
]
+error: main: types don't match in call for 'foo x'
+error:   which tries to call 'recipe foo x:&:char'

:(scenario show_available_variants_in_dispatch_errors)
% Hide_errors = true;
def main [
  local-scope
  x:&:num <- copy 0
  foo x
]
def foo x:&:char [
  local-scope
  load-ingredients
]
def foo x:&:bool [
  local-scope
  load-ingredients
]
+error: main: failed to find a matching call for 'foo x'
+error:   available variants are:
+error:     recipe foo x:&:char
+error:     recipe foo x:&:bool

:(before "End Includes")
using std::abs;
