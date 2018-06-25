//: Ingredients of a recipe are meant to be immutable unless they're also
//: products. This layer will start enforcing this check.
//:
//: One hole for now: variables in surrounding spaces are implicitly mutable.
//: [tag: todo]

:(scenario can_modify_ingredients_that_are_also_products)
# mutable container
def main [
  local-scope
  p:point <- merge 34, 35
  p <- foo p
]
def foo p:point -> p:point [
  local-scope
  load-ingredients
  p <- put p, x:offset, 34
]
$error: 0

:(scenario can_modify_ingredients_that_are_also_products_2)
def main [
  local-scope
  p:&:point <- new point:type
  p <- foo p
]
# mutable address to container
def foo p:&:point -> p:&:point [
  local-scope
  load-ingredients
  *p <- put *p, x:offset, 34
]
$error: 0

:(scenario can_modify_ingredients_that_are_also_products_3)
def main [
  local-scope
  p:&:@:num <- new number:type, 3
  p <- foo p
]
# mutable address
def foo p:&:@:num -> p:&:@:num [
  local-scope
  load-ingredients
  *p <- put-index *p, 0, 34
]
$error: 0

:(scenario ignore_literal_ingredients_for_immutability_checks)
def main [
  local-scope
  p:&:d1 <- new d1:type
  q:num <- foo p
]
def foo p:&:d1 -> q:num [
  local-scope
  load-ingredients
  x:&:d1 <- new d1:type
  *x <- put *x, p:offset, 34  # ignore this 'p'
  return 36
]
container d1 [
  p:num
  q:num
]
$error: 0

:(scenario cannot_modify_immutable_ingredients)
% Hide_errors = true;
def main [
  local-scope
  x:&:num <- new number:type
  foo x
]
# immutable address to primitive
def foo x:&:num [
  local-scope
  load-ingredients
  *x <- copy 34
]
+error: foo: cannot modify 'x' in instruction '*x <- copy 34' because it's an ingredient of recipe foo but not also a product

:(scenario cannot_modify_immutable_containers)
% Hide_errors = true;
def main [
  local-scope
  x:point-number <- merge 34, 35, 36
  foo x
]
# immutable container
def foo x:point-number [
  local-scope
  load-ingredients
  # copy an element: ok
  y:point <- get x, xy:offset
  # modify the element: boom
  # This could be ok if y contains no addresses, but we're not going to try to be that smart.
  # It also makes the rules easier to reason about. If it's just an ingredient, just don't try to change it.
  y <- put y, x:offset, 37
]
+error: foo: cannot modify 'y' in instruction 'y <- put y, x:offset, 37' because that would modify 'x' which is an ingredient of recipe foo but not also a product

:(scenario can_modify_immutable_pointers)
def main [
  local-scope
  x:&:num <- new number:type
  foo x
]
def foo x:&:num [
  local-scope
  load-ingredients
  # modify the address, not the payload
  x <- copy null
]
$error: 0

:(scenario can_modify_immutable_pointers_but_not_their_payloads)
% Hide_errors = true;
def main [
  local-scope
  x:&:num <- new number:type
  foo x
]
def foo x:&:num [
  local-scope
  load-ingredients
  # modify address; ok
  x <- new number:type
  # modify payload: boom
  # this could be ok, but we're not going to try to be that smart
  *x <- copy 34
]
+error: foo: cannot modify 'x' in instruction '*x <- copy 34' because it's an ingredient of recipe foo but not also a product

:(scenario cannot_call_mutating_recipes_on_immutable_ingredients)
% Hide_errors = true;
def main [
  local-scope
  p:&:point <- new point:type
  foo p
]
def foo p:&:point [
  local-scope
  load-ingredients
  bar p
]
def bar p:&:point -> p:&:point [
  local-scope
  load-ingredients
  # p could be modified here, but it doesn't have to be, it's already marked
  # mutable in the header
]
+error: foo: cannot modify 'p' in instruction 'bar p' because it's an ingredient of recipe foo but not also a product

:(scenario cannot_modify_copies_of_immutable_ingredients)
% Hide_errors = true;
def main [
  local-scope
  p:&:point <- new point:type
  foo p
]
def foo p:&:point [
  local-scope
  load-ingredients
  q:&:point <- copy p
  *q <- put *q, x:offset, 34
]
+error: foo: cannot modify 'q' in instruction '*q <- put *q, x:offset, 34' because that would modify p which is an ingredient of recipe foo but not also a product

:(scenario can_modify_copies_of_mutable_ingredients)
def main [
  local-scope
  p:&:point <- new point:type
  foo p
]
def foo p:&:point -> p:&:point [
  local-scope
  load-ingredients
  q:&:point <- copy p
  *q <- put *q, x:offset, 34
]
$error: 0

:(scenario cannot_modify_address_inside_immutable_ingredients)
% Hide_errors = true;
container foo [
  x:&:@:num  # contains an address
]
def main [
  # don't run anything
]
def foo a:&:foo [
  local-scope
  load-ingredients
  x:&:@:num <- get *a, x:offset  # just a regular get of the container
  *x <- put-index *x, 0, 34  # but then a put-index on the result
]
+error: foo: cannot modify 'x' in instruction '*x <- put-index *x, 0, 34' because that would modify a which is an ingredient of recipe foo but not also a product

:(scenario cannot_modify_address_inside_immutable_ingredients_2)
container foo [
  x:&:@:num  # contains an address
]
def main [
  # don't run anything
]
def foo a:&:foo [
  local-scope
  load-ingredients
  b:foo <- merge null
  # modify b, completely unrelated to immutable ingredient a
  x:&:@:num <- get b, x:offset
  *x <- put-index *x, 0, 34
]
$error: 0

:(scenario cannot_modify_address_inside_immutable_ingredients_3)
% Hide_errors = true;
def main [
  # don't run anything
]
def foo a:&:@:&:num [
  local-scope
  load-ingredients
  x:&:num <- index *a, 0  # just a regular index of the array
  *x <- copy 34  # but then modify the result
]
+error: foo: cannot modify 'x' in instruction '*x <- copy 34' because that would modify a which is an ingredient of recipe foo but not also a product

:(scenario cannot_modify_address_inside_immutable_ingredients_4)
def main [
  # don't run anything
]
def foo a:&:@:&:num [
  local-scope
  load-ingredients
  b:&:@:&:num <- new {(address number): type}, 3
  # modify b, completely unrelated to immutable ingredient a
  x:&:num <- index *b, 0
  *x <- copy 34
]
$error: 0

:(scenario latter_ingredient_of_index_is_immutable)
def main [
  # don't run anything
]
def foo a:&:@:&:@:num, b:num -> a:&:@:&:@:num [
  local-scope
  load-ingredients
  x:&:@:num <- index *a, b
  *x <- put-index *x, 0, 34
]
$error: 0

:(scenario can_traverse_immutable_ingredients)
container test-list [
  next:&:test-list
]
def main [
  local-scope
  p:&:test-list <- new test-list:type
  foo p
]
def foo p:&:test-list [
  local-scope
  load-ingredients
  p2:&:test-list <- bar p
]
def bar x:&:test-list -> y:&:test-list [
  local-scope
  load-ingredients
  y <- get *x, next:offset
]
$error: 0

:(scenario treat_optional_ingredients_as_mutable)
def main [
  k:&:num <- new number:type
  test k
]
# recipe taking an immutable address ingredient
def test k:&:num [
  local-scope
  load-ingredients
  foo k
]
# ..calling a recipe with an optional address ingredient
def foo -> [
  local-scope
  load-ingredients
  k:&:num, found?:bool <- next-ingredient
  # we don't further check k for immutability, but assume it's mutable
]
$error: 0

:(scenario treat_optional_ingredients_as_mutable_2)
% Hide_errors = true;
def main [
  local-scope
  p:&:point <- new point:type
  foo p
]
def foo p:&:point [
  local-scope
  load-ingredients
  bar p
]
def bar [
  local-scope
  load-ingredients
  p:&:point <- next-ingredient  # optional ingredient; assumed to be mutable
]
+error: foo: cannot modify 'p' in instruction 'bar p' because it's an ingredient of recipe foo but not also a product

//: when checking for immutable ingredients, remember to take space into account
:(scenario check_space_of_reagents_in_immutability_checks)
def main [
  a:space/names:new-closure <- new-closure
  b:&:num <- new number:type
  run-closure b:&:num, a:space
]
def new-closure [
  local-scope
  x:&:num <- new number:type
  return default-space/names:new-closure
]
def run-closure x:&:num, s:space/names:new-closure [
  local-scope
  load-ingredients
  0:space/names:new-closure <- copy s
  # different space; always mutable
  *x:&:num/space:1 <- copy 34
]
$error: 0

:(before "End Transforms")
Transform.push_back(check_immutable_ingredients);  // idempotent

:(code)
void check_immutable_ingredients(const recipe_ordinal r) {
  // to ensure an address reagent isn't modified, it suffices to show that
  //   a) we never write to its contents directly,
  //   b) we never call 'put' or 'put-index' on it, and
  //   c) any non-primitive recipe calls in the body aren't returning it as a product
  const recipe& caller = get(Recipe, r);
  trace(9991, "transform") << "--- check mutability of ingredients in recipe " << caller.name << end();
  if (!caller.has_header) return;  // skip check for old-style recipes calling next-ingredient directly
  for (int i = 0;  i < SIZE(caller.ingredients);  ++i) {
    const reagent& current_ingredient = caller.ingredients.at(i);
    if (is_present_in_products(caller, current_ingredient.name)) continue;  // not expected to be immutable
    // End Immutable Ingredients Special-cases
    set<reagent, name_and_space_lt> immutable_vars;
    immutable_vars.insert(current_ingredient);
    for (int i = 0;  i < SIZE(caller.steps);  ++i) {
      const instruction& inst = caller.steps.at(i);
      check_immutable_ingredient_in_instruction(inst, immutable_vars, current_ingredient.name, caller);
      if (inst.operation == INDEX && SIZE(inst.ingredients) > 1 && inst.ingredients.at(1).name == current_ingredient.name) continue;
      update_aliases(inst, immutable_vars);
    }
  }
}

void update_aliases(const instruction& inst, set<reagent, name_and_space_lt>& current_ingredient_and_aliases) {
  set<int> current_ingredient_indices = ingredient_indices(inst, current_ingredient_and_aliases);
  if (!contains_key(Recipe, inst.operation)) {
    // primitive recipe
    switch (inst.operation) {
      case COPY:
        for (set<int>::iterator p = current_ingredient_indices.begin();  p != current_ingredient_indices.end();  ++p)
          current_ingredient_and_aliases.insert(inst.products.at(*p).name);
        break;
      case GET:
      case INDEX:
      case MAYBE_CONVERT:
        // current_ingredient_indices can only have 0 or one value
        if (!current_ingredient_indices.empty() && !inst.products.empty()) {
          if (is_mu_address(inst.products.at(0)) || is_mu_container(inst.products.at(0)) || is_mu_exclusive_container(inst.products.at(0)))
            current_ingredient_and_aliases.insert(inst.products.at(0));
        }
        break;
      default: break;
    }
  }
  else {
    // defined recipe
    set<int> contained_in_product_indices = scan_contained_in_product_indices(inst, current_ingredient_indices);
    for (set<int>::iterator p = contained_in_product_indices.begin();  p != contained_in_product_indices.end();  ++p) {
      if (*p < SIZE(inst.products))
        current_ingredient_and_aliases.insert(inst.products.at(*p));
    }
  }
}

set<int> scan_contained_in_product_indices(const instruction& inst, set<int>& ingredient_indices) {
  set<reagent, name_and_space_lt> selected_ingredients;
  const recipe& callee = get(Recipe, inst.operation);
  for (set<int>::iterator p = ingredient_indices.begin();  p != ingredient_indices.end();  ++p) {
    if (*p >= SIZE(callee.ingredients)) continue;  // optional immutable ingredient
    selected_ingredients.insert(callee.ingredients.at(*p));
  }
  set<int> result;
  for (int i = 0;  i < SIZE(callee.products);  ++i) {
    const reagent& current_product = callee.products.at(i);
    const string_tree* contained_in_name = property(current_product, "contained-in");
    if (contained_in_name && selected_ingredients.find(contained_in_name->value) != selected_ingredients.end())
      result.insert(i);
  }
  return result;
}

bool is_mu_container(const reagent& r) {
  return is_mu_container(r.type);
}
bool is_mu_container(const type_tree* type) {
  if (!type) return false;
  if (!type->atom)
    return is_mu_container(get_base_type(type));
  if (type->value == 0) return false;
  if (!contains_key(Type, type->value)) return false;  // error raised elsewhere
  type_info& info = get(Type, type->value);
  return info.kind == CONTAINER;
}

bool is_mu_exclusive_container(const reagent& r) {
  return is_mu_exclusive_container(r.type);
}
bool is_mu_exclusive_container(const type_tree* type) {
  if (!type) return false;
  if (!type->atom)
    return is_mu_exclusive_container(get_base_type(type));
  if (type->value == 0) return false;
  if (!contains_key(Type, type->value)) return false;  // error raised elsewhere
  type_info& info = get(Type, type->value);
  return info.kind == EXCLUSIVE_CONTAINER;
}

:(before "End Types")
// reagent comparison -- only in the context of a single recipe
struct name_and_space_lt {
  bool operator()(const reagent& a, const reagent& b) const;
};
:(code)
bool name_and_space_lt::operator()(const reagent& a, const reagent& b) const {
  int aspace = 0, bspace = 0;
  if (has_property(a, "space")) aspace = to_integer(property(a, "space")->value);
  if (has_property(b, "space")) bspace = to_integer(property(b, "space")->value);
  if (aspace != bspace) return aspace < bspace;
  return a.name < b.name;
}

:(scenarios transform)
:(scenario immutability_infects_contained_in_variables)
% Hide_errors = true;
container test-list [
  value:num
  next:&:test-list
]
def main [
  local-scope
  p:&:test-list <- new test-list:type
  foo p
]
def foo p:&:test-list [  # p is immutable
  local-scope
  load-ingredients
  p2:&:test-list <- test-next p  # p2 is immutable
  *p2 <- put *p2, value:offset, 34
]
def test-next x:&:test-list -> y:&:test-list/contained-in:x [
  local-scope
  load-ingredients
  y <- get *x, next:offset
]
+error: foo: cannot modify 'p2' in instruction '*p2 <- put *p2, value:offset, 34' because that would modify p which is an ingredient of recipe foo but not also a product

:(code)
void check_immutable_ingredient_in_instruction(const instruction& inst, const set<reagent, name_and_space_lt>& current_ingredient_and_aliases, const string& original_ingredient_name, const recipe& caller) {
  // first check if the instruction is directly modifying something it shouldn't
  for (int i = 0;  i < SIZE(inst.products);  ++i) {
    if (has_property(inst.products.at(i), "lookup")
        && current_ingredient_and_aliases.find(inst.products.at(i)) != current_ingredient_and_aliases.end()) {
      string current_product_name = inst.products.at(i).name;
      if (current_product_name == original_ingredient_name)
        raise << maybe(caller.name) << "cannot modify '" << current_product_name << "' in instruction '" << to_original_string(inst) << "' because it's an ingredient of recipe " << caller.name << " but not also a product\n" << end();
      else
        raise << maybe(caller.name) << "cannot modify '" << current_product_name << "' in instruction '" << to_original_string(inst) << "' because that would modify " << original_ingredient_name << " which is an ingredient of recipe " << caller.name << " but not also a product\n" << end();
      return;
    }
  }
  // check if there's any indirect modification going on
  set<int> current_ingredient_indices = ingredient_indices(inst, current_ingredient_and_aliases);
  if (current_ingredient_indices.empty()) return;  // ingredient not found in call
  for (set<int>::iterator p = current_ingredient_indices.begin();  p != current_ingredient_indices.end();  ++p) {
    const int current_ingredient_index = *p;
    reagent current_ingredient = inst.ingredients.at(current_ingredient_index);
    canonize_type(current_ingredient);
    const string& current_ingredient_name = current_ingredient.name;
    if (!contains_key(Recipe, inst.operation)) {
      // primitive recipe
      // we got here only because we got an instruction with an implicit product, and the instruction didn't explicitly spell it out
      //    put x, y:offset, z
      // instead of
      //    x <- put x, y:offset, z
      if (inst.operation == PUT || inst.operation == PUT_INDEX) {
        if (current_ingredient_index == 0) {
          if (current_ingredient_name == original_ingredient_name)
            raise << maybe(caller.name) << "cannot modify '" << current_ingredient_name << "' in instruction '" << to_original_string(inst) << "' because it's an ingredient of recipe " << caller.name << " but not also a product\n" << end();
          else
            raise << maybe(caller.name) << "cannot modify '" << current_ingredient_name << "' in instruction '" << to_original_string(inst) << "' because that would modify '" << original_ingredient_name << "' which is an ingredient of recipe " << caller.name << " but not also a product\n" << end();
        }
      }
    }
    else {
      // defined recipe
      if (is_modified_in_recipe(inst.operation, current_ingredient_index, caller)) {
        if (current_ingredient_name == original_ingredient_name)
          raise << maybe(caller.name) << "cannot modify '" << current_ingredient_name << "' in instruction '" << to_original_string(inst) << "' because it's an ingredient of recipe " << caller.name << " but not also a product\n" << end();
        else
          raise << maybe(caller.name) << "cannot modify '" << current_ingredient_name << "' in instruction '" << to_original_string(inst) << "' because that would modify '" << original_ingredient_name << "' which is an ingredient of recipe " << caller.name << " but not also a product\n" << end();
      }
    }
  }
}

bool is_modified_in_recipe(const recipe_ordinal r, const int ingredient_index, const recipe& caller) {
  const recipe& callee = get(Recipe, r);
  if (!callee.has_header) {
    raise << maybe(caller.name) << "can't check mutability of ingredients in recipe " << callee.name << " because it uses 'next-ingredient' directly, rather than a recipe header.\n" << end();
    return true;
  }
  if (ingredient_index >= SIZE(callee.ingredients)) return false;  // optional immutable ingredient
  return is_present_in_products(callee, callee.ingredients.at(ingredient_index).name);
}

bool is_present_in_products(const recipe& callee, const string& ingredient_name) {
  for (int i = 0;  i < SIZE(callee.products);  ++i) {
    if (callee.products.at(i).name == ingredient_name)
      return true;
  }
  return false;
}

set<int> ingredient_indices(const instruction& inst, const set<reagent, name_and_space_lt>& ingredient_names) {
  set<int> result;
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (is_literal(inst.ingredients.at(i))) continue;
    if (ingredient_names.find(inst.ingredients.at(i)) != ingredient_names.end())
      result.insert(i);
  }
  return result;
}

//: Sometimes you want to pass in two addresses, one pointing inside the
//: other. For example, you want to delete a node from a linked list. You
//: can't pass both pointers back out, because if a caller tries to make both
//: identical then you can't tell which value will be written on the way out.
//:
//: Experimental solution: just tell Mu that one points inside the other.
//: This way we can return just one pointer as high up as necessary to capture
//: all modifications performed by a recipe.
//:
//: We'll see if we end up wanting to abuse /contained-in for other reasons.

:(scenarios transform)
:(scenario can_modify_contained_in_addresses)
container test-list [
  value:num
  next:&:test-list
]
def main [
  local-scope
  p:&:test-list <- new test-list:type
  foo p
]
def foo p:&:test-list -> p:&:test-list [
  local-scope
  load-ingredients
  p2:&:test-list <- test-next p
  p <- test-remove p2, p
]
def test-next x:&:test-list -> y:&:test-list [
  local-scope
  load-ingredients
  y <- get *x, next:offset
]
def test-remove x:&:test-list/contained-in:from, from:&:test-list -> from:&:test-list [
  local-scope
  load-ingredients
  *x <- put *x, value:offset, 34  # can modify x
]
$error: 0

:(before "End Immutable Ingredients Special-cases")
if (has_property(current_ingredient, "contained-in")) {
  const string_tree* tmp = property(current_ingredient, "contained-in");
  if (!tmp->atom
      || (!is_present_in_ingredients(caller, tmp->value)
          && !is_present_in_products(caller, tmp->value))) {
    raise << maybe(caller.name) << "/contained-in can only point to another ingredient or product, but got '" << to_string(property(current_ingredient, "contained-in")) << "'\n" << end();
  }
  continue;
}

:(scenario contained_in_product)
container test-list [
  value:num
  next:&:test-list
]
def foo x:&:test-list/contained-in:result -> result:&:test-list [
  local-scope
  load-ingredients
  result <- copy null
]
$error: 0

:(scenario contained_in_is_mutable)
container test-list [
  value:num
  next:&:test-list
]
def foo x:&:test-list/contained-in:result -> result:&:test-list [
  local-scope
  load-ingredients
  result <- copy x
  put *x, value:offset, 34
]
$error: 0
