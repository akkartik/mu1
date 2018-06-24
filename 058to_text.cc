//: Primitive to convert any type to text (array of characters).
//: Later layers will allow us to override this to do something smarter for
//: specific types.

:(before "End Primitive Recipe Declarations")
TO_TEXT,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "to-text", TO_TEXT);
:(before "End Primitive Recipe Checks")
case TO_TEXT: {
  if (SIZE(inst.ingredients) != 1) {
    raise << maybe(get(Recipe, r).name) << "'to-text' requires a single ingredient, but got '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  // can handle any type
  break;
}
:(before "End Primitive Recipe Implementations")
case TO_TEXT: {
  products.resize(1);
  products.at(0).push_back(/*alloc id*/0);
  products.at(0).push_back(new_mu_text(inspect(current_instruction().ingredients.at(0), ingredients.at(0))));
  break;
}
