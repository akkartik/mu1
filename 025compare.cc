//: Comparison primitives

:(before "End Primitive Recipe Declarations")
EQUAL,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "equal", EQUAL);
:(before "End Primitive Recipe Checks")
case EQUAL: {
  if (SIZE(inst.ingredients) <= 1) {
    raise << maybe(get(Recipe, r).name) << "'equal' needs at least two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  const reagent& exemplar = inst.ingredients.at(0);
  for (int i = /*skip exemplar*/1;  i < SIZE(inst.ingredients);  ++i) {
    if (!types_match(inst.ingredients.at(i), exemplar) && !types_match(exemplar, inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'equal' expects ingredients to be all of the same type, but got '" << to_original_string(inst) << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'equal' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_boolean(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'equal' should yield a boolean, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case EQUAL: {
  // todo: keep the address exception from slowing down the common case
  drop_alloc_ids_if_comparing_address_to_literal_0(ingredients);
  vector<double>& exemplar = ingredients.at(0);
  bool result = true;
  for (int i = /*skip exemplar*/1;  i < SIZE(ingredients);  ++i) {
    if (!equal(ingredients.at(i).begin(), ingredients.at(i).end(), exemplar.begin())) {
      result = false;
      break;
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}
:(code)
void drop_alloc_ids_if_comparing_address_to_literal_0(vector<vector<double> >& ingredients) {
  bool any_ingredient_is_null = false;
  bool any_ingredient_is_address = false;
  for (int i = 0;  i < SIZE(current_instruction().ingredients);  ++i) {
    if (current_instruction().ingredients.at(i).name == "0")
      any_ingredient_is_null = true;
    if (is_mu_address(current_instruction().ingredients.at(i)))
      any_ingredient_is_address = true;
  }
  if (any_ingredient_is_null && any_ingredient_is_address) {
    for (int i = 0;  i < SIZE(ingredients);  ++i) {
      if (is_mu_address(current_instruction().ingredients.at(i)))
        ingredients.at(i).erase(ingredients.at(i).begin());
    }
  }
}

:(scenario equal)
def main [
  1:num <- copy 34
  2:num <- copy 33
  3:bool <- equal 1:num, 2:num
]
+mem: location 1 is 34
+mem: location 2 is 33
+mem: storing 0 in location 3

:(scenario equal_2)
def main [
  1:num <- copy 34
  2:num <- copy 34
  3:bool <- equal 1:num, 2:num
]
+mem: location 1 is 34
+mem: location 2 is 34
+mem: storing 1 in location 3

:(scenario equal_multiple)
def main [
  1:bool <- equal 34, 34, 34
]
+mem: storing 1 in location 1

:(scenario equal_multiple_2)
def main [
  1:bool <- equal 34, 34, 35
]
+mem: storing 0 in location 1

:(scenario equal_address_null)
def main [
  1:&:num <- copy 0
  10:bool <- equal 1:&:num, 0
]
+mem: storing 1 in location 10

:(scenario equal_address_null_2)
def main [
  1:&:num <- copy 0
  10:bool <- equal 0, 1:&:num
]
+mem: storing 1 in location 10

:(scenario equal_address_null_3)
def main [
  1:&:num <- new num:type
  10:bool <- equal 1:&:num, 0
]
+mem: storing 0 in location 10

:(scenario equal_address_null_multiple)
def main [
  1:&:num <- copy 0
  10:bool <- equal 0, 1:&:num, 0
]
+mem: storing 1 in location 10

:(scenario equal_address_null_multiple_2)
def main [
  1:&:num <- copy 0
  3:&:num <- copy 0
  10:bool <- equal 0, 1:&:num, 0, 3:&:num
]
+mem: storing 1 in location 10

:(before "End Primitive Recipe Declarations")
NOT_EQUAL,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "not-equal", NOT_EQUAL);
:(before "End Primitive Recipe Checks")
case NOT_EQUAL: {
  if (SIZE(inst.ingredients) != 2) {
    raise << maybe(get(Recipe, r).name) << "'equal' needs two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  const reagent& exemplar = inst.ingredients.at(0);
  if (!types_match(inst.ingredients.at(1), exemplar) && !types_match(exemplar, inst.ingredients.at(1))) {
    raise << maybe(get(Recipe, r).name) << "'equal' expects ingredients to be all of the same type, but got '" << to_original_string(inst) << "'\n" << end();
    goto finish_checking_instruction;
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'equal' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_boolean(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'equal' should yield a boolean, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case NOT_EQUAL: {
  // todo: keep the address exception from slowing down the common case
  drop_alloc_ids_if_comparing_address_to_literal_0(ingredients);
  vector<double>& exemplar = ingredients.at(0);
  products.resize(1);
  bool equal_ingredients = equal(ingredients.at(1).begin(), ingredients.at(1).end(), exemplar.begin());
  products.at(0).push_back(!equal_ingredients);
  break;
}

:(scenario not_equal)
def main [
  1:num <- copy 34
  2:num <- copy 33
  3:bool <- not-equal 1:num, 2:num
]
+mem: location 1 is 34
+mem: location 2 is 33
+mem: storing 1 in location 3

:(scenario not_equal_2)
def main [
  1:num <- copy 34
  2:num <- copy 34
  3:bool <- not-equal 1:num, 2:num
]
+mem: location 1 is 34
+mem: location 2 is 34
+mem: storing 0 in location 3

:(before "End Primitive Recipe Declarations")
GREATER_THAN,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "greater-than", GREATER_THAN);
:(before "End Primitive Recipe Checks")
case GREATER_THAN: {
  if (SIZE(inst.ingredients) <= 1) {
    raise << maybe(get(Recipe, r).name) << "'greater-than' needs at least two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_number(inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'greater-than' can only compare numbers; got '" << inst.ingredients.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'greater-than' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_boolean(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'greater-than' should yield a boolean, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case GREATER_THAN: {
  bool result = true;
  for (int i = /**/1;  i < SIZE(ingredients);  ++i) {
    if (ingredients.at(i-1).at(0) <= ingredients.at(i).at(0)) {
      result = false;
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}

:(scenario greater_than)
def main [
  1:num <- copy 34
  2:num <- copy 33
  3:bool <- greater-than 1:num, 2:num
]
+mem: storing 1 in location 3

:(scenario greater_than_2)
def main [
  1:num <- copy 34
  2:num <- copy 34
  3:bool <- greater-than 1:num, 2:num
]
+mem: storing 0 in location 3

:(scenario greater_than_multiple)
def main [
  1:bool <- greater-than 36, 35, 34
]
+mem: storing 1 in location 1

:(scenario greater_than_multiple_2)
def main [
  1:bool <- greater-than 36, 35, 35
]
+mem: storing 0 in location 1

:(before "End Primitive Recipe Declarations")
LESSER_THAN,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "lesser-than", LESSER_THAN);
:(before "End Primitive Recipe Checks")
case LESSER_THAN: {
  if (SIZE(inst.ingredients) <= 1) {
    raise << maybe(get(Recipe, r).name) << "'lesser-than' needs at least two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_number(inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'lesser-than' can only compare numbers; got '" << inst.ingredients.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'lesser-than' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_boolean(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'lesser-than' should yield a boolean, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case LESSER_THAN: {
  bool result = true;
  for (int i = /**/1;  i < SIZE(ingredients);  ++i) {
    if (ingredients.at(i-1).at(0) >= ingredients.at(i).at(0)) {
      result = false;
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}

:(scenario lesser_than)
def main [
  1:num <- copy 32
  2:num <- copy 33
  3:bool <- lesser-than 1:num, 2:num
]
+mem: storing 1 in location 3

:(scenario lesser_than_2)
def main [
  1:num <- copy 34
  2:num <- copy 33
  3:bool <- lesser-than 1:num, 2:num
]
+mem: storing 0 in location 3

:(scenario lesser_than_multiple)
def main [
  1:bool <- lesser-than 34, 35, 36
]
+mem: storing 1 in location 1

:(scenario lesser_than_multiple_2)
def main [
  1:bool <- lesser-than 34, 35, 35
]
+mem: storing 0 in location 1

:(before "End Primitive Recipe Declarations")
GREATER_OR_EQUAL,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "greater-or-equal", GREATER_OR_EQUAL);
:(before "End Primitive Recipe Checks")
case GREATER_OR_EQUAL: {
  if (SIZE(inst.ingredients) <= 1) {
    raise << maybe(get(Recipe, r).name) << "'greater-or-equal' needs at least two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_number(inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'greater-or-equal' can only compare numbers; got '" << inst.ingredients.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'greater-or-equal' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_boolean(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'greater-or-equal' should yield a boolean, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case GREATER_OR_EQUAL: {
  bool result = true;
  for (int i = /**/1;  i < SIZE(ingredients);  ++i) {
    if (ingredients.at(i-1).at(0) < ingredients.at(i).at(0)) {
      result = false;
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}

:(scenario greater_or_equal)
def main [
  1:num <- copy 34
  2:num <- copy 33
  3:bool <- greater-or-equal 1:num, 2:num
]
+mem: storing 1 in location 3

:(scenario greater_or_equal_2)
def main [
  1:num <- copy 34
  2:num <- copy 34
  3:bool <- greater-or-equal 1:num, 2:num
]
+mem: storing 1 in location 3

:(scenario greater_or_equal_3)
def main [
  1:num <- copy 34
  2:num <- copy 35
  3:bool <- greater-or-equal 1:num, 2:num
]
+mem: storing 0 in location 3

:(scenario greater_or_equal_multiple)
def main [
  1:bool <- greater-or-equal 36, 35, 35
]
+mem: storing 1 in location 1

:(scenario greater_or_equal_multiple_2)
def main [
  1:bool <- greater-or-equal 36, 35, 36
]
+mem: storing 0 in location 1

:(before "End Primitive Recipe Declarations")
LESSER_OR_EQUAL,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "lesser-or-equal", LESSER_OR_EQUAL);
:(before "End Primitive Recipe Checks")
case LESSER_OR_EQUAL: {
  if (SIZE(inst.ingredients) <= 1) {
    raise << maybe(get(Recipe, r).name) << "'lesser-or-equal' needs at least two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_number(inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'lesser-or-equal' can only compare numbers; got '" << inst.ingredients.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'greater-or-equal' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_boolean(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'greater-or-equal' should yield a boolean, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case LESSER_OR_EQUAL: {
  bool result = true;
  for (int i = /**/1;  i < SIZE(ingredients);  ++i) {
    if (ingredients.at(i-1).at(0) > ingredients.at(i).at(0)) {
      result = false;
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}

:(scenario lesser_or_equal)
def main [
  1:num <- copy 32
  2:num <- copy 33
  3:bool <- lesser-or-equal 1:num, 2:num
]
+mem: storing 1 in location 3

:(scenario lesser_or_equal_2)
def main [
  1:num <- copy 33
  2:num <- copy 33
  3:bool <- lesser-or-equal 1:num, 2:num
]
+mem: storing 1 in location 3

:(scenario lesser_or_equal_3)
def main [
  1:num <- copy 34
  2:num <- copy 33
  3:bool <- lesser-or-equal 1:num, 2:num
]
+mem: storing 0 in location 3

:(scenario lesser_or_equal_multiple)
def main [
  1:bool <- lesser-or-equal 34, 35, 35
]
+mem: storing 1 in location 1

:(scenario lesser_or_equal_multiple_2)
def main [
  1:bool <- lesser-or-equal 34, 35, 34
]
+mem: storing 0 in location 1

:(before "End Primitive Recipe Declarations")
MAX,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "max", MAX);
:(before "End Primitive Recipe Checks")
case MAX: {
  if (SIZE(inst.ingredients) <= 1) {
    raise << maybe(get(Recipe, r).name) << "'max' needs at least two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_number(inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'max' can only compare numbers; got '" << inst.ingredients.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'max' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_number(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'max' should yield a number, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case MAX: {
  int result = ingredients.at(0).at(0);
  for (int i = /**/1;  i < SIZE(ingredients);  ++i) {
    if (ingredients.at(i).at(0) > result) {
      result = ingredients.at(i).at(0);
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}

:(before "End Primitive Recipe Declarations")
MIN,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "min", MIN);
:(before "End Primitive Recipe Checks")
case MIN: {
  if (SIZE(inst.ingredients) <= 1) {
    raise << maybe(get(Recipe, r).name) << "'min' needs at least two ingredients to compare in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_number(inst.ingredients.at(i))) {
      raise << maybe(get(Recipe, r).name) << "'min' can only compare numbers; got '" << inst.ingredients.at(i).original_string << "'\n" << end();
      goto finish_checking_instruction;
    }
  }
  if (SIZE(inst.products) > 1) {
    raise << maybe(get(Recipe, r).name) << "'min' yields exactly one product in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!inst.products.empty() && !is_dummy(inst.products.at(0)) && !is_mu_number(inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'min' should yield a number, but got '" << inst.products.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case MIN: {
  int result = ingredients.at(0).at(0);
  for (int i = /**/1;  i < SIZE(ingredients);  ++i) {
    if (ingredients.at(i).at(0) < result) {
      result = ingredients.at(i).at(0);
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}
