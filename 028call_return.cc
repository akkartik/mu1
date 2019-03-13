//: Calls can also generate products, using 'reply' or 'return'.

void test_return() {
  run(
      "def main [\n"
      "  1:num, 2:num <- f 34\n"
      "]\n"
      "def f [\n"
      "  12:num <- next-ingredient\n"
      "  13:num <- add 1, 12:num\n"
      "  return 12:num, 13:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
      "mem: storing 35 in location 2\n"
  );
}

void test_reply() {
  run(
      "def main [\n"
      "  1:num, 2:num <- f 34\n"
      "]\n"
      "def f [\n"
      "  12:num <- next-ingredient\n"
      "  13:num <- add 1, 12:num\n"
      "  reply 12:num, 13:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
      "mem: storing 35 in location 2\n"
  );
}

:(before "End Primitive Recipe Declarations")
RETURN,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "return", RETURN);
put(Recipe_ordinal, "reply", RETURN);  // synonym while teaching
put(Recipe_ordinal, "output", RETURN);  // experiment
:(before "End Primitive Recipe Checks")
case RETURN: {
  break;  // checks will be performed by a transform below
}
:(before "End Primitive Recipe Implementations")
case RETURN: {
  // Begin Return
  trace(Callstack_depth+1, "trace") << current_instruction().name << ": decrementing callstack depth from " << Callstack_depth << end();
  --Callstack_depth;
  if (Callstack_depth < 0) {
    Current_routine->calls.clear();
    goto stop_running_current_routine;
  }
  Current_routine->calls.pop_front();
  // just in case 'main' returns a value, drop it for now
  if (Current_routine->calls.empty()) goto stop_running_current_routine;
  for (int i = 0;  i < SIZE(ingredients);  ++i)
    trace(Callstack_depth+1, "run") << "result " << i << " is " << to_string(ingredients.at(i)) << end();
  // make return products available to caller
  copy(ingredients.begin(), ingredients.end(), inserter(products, products.begin()));
  // End Return
  break;  // continue to process rest of *caller* instruction
}

//: Types in return instructions are checked ahead of time.

:(before "End Checks")
Transform.push_back(check_types_of_return_instructions);  // idempotent
:(code)
void check_types_of_return_instructions(const recipe_ordinal r) {
  const recipe& caller = get(Recipe, r);
  trace(9991, "transform") << "--- check types of return instructions in recipe " << caller.name << end();
  for (int i = 0;  i < SIZE(caller.steps);  ++i) {
    const instruction& caller_instruction = caller.steps.at(i);
    if (caller_instruction.is_label) continue;
    if (caller_instruction.products.empty()) continue;
    if (is_primitive(caller_instruction.operation)) continue;
    const recipe& callee = get(Recipe, caller_instruction.operation);
    for (int i = 0;  i < SIZE(callee.steps);  ++i) {
      const instruction& return_inst = callee.steps.at(i);
      if (return_inst.operation != RETURN) continue;
      // check types with the caller
      if (SIZE(caller_instruction.products) > SIZE(return_inst.ingredients)) {
        raise << maybe(caller.name) << "too few values returned from " << callee.name << '\n' << end();
        break;
      }
      for (int i = 0;  i < SIZE(caller_instruction.products);  ++i) {
        reagent/*copy*/ lhs = return_inst.ingredients.at(i);
        reagent/*copy*/ rhs = caller_instruction.products.at(i);
        // End Check RETURN Copy(lhs, rhs)
        if (!types_coercible(rhs, lhs)) {
          raise << maybe(callee.name) << return_inst.name << " ingredient '" << lhs.original_string << "' can't be saved in '" << rhs.original_string << "'\n" << end();
          raise << "  ['" << to_string(lhs.type) << "' vs '" << to_string(rhs.type) << "']\n" << end();
          goto finish_return_check;
        }
      }
      // check that any return ingredients with /same-as-ingredient connect up
      // the corresponding ingredient and product in the caller.
      for (int i = 0;  i < SIZE(caller_instruction.products);  ++i) {
        if (has_property(return_inst.ingredients.at(i), "same-as-ingredient")) {
          string_tree* tmp = property(return_inst.ingredients.at(i), "same-as-ingredient");
          if (!tmp || !tmp->atom) {
            raise << maybe(caller.name) << "'same-as-ingredient' metadata should take exactly one value in '" << to_original_string(return_inst) << "'\n" << end();
            goto finish_return_check;
          }
          int ingredient_index = to_integer(tmp->value);
          if (ingredient_index >= SIZE(caller_instruction.ingredients)) {
            raise << maybe(caller.name) << "too few ingredients in '" << to_original_string(caller_instruction) << "'\n" << end();
            goto finish_return_check;
          }
          if (!is_dummy(caller_instruction.products.at(i)) && !is_literal(caller_instruction.ingredients.at(ingredient_index)) && caller_instruction.products.at(i).name != caller_instruction.ingredients.at(ingredient_index).name) {
            raise << maybe(caller.name) << "'" << to_original_string(caller_instruction) << "' should write to '" << caller_instruction.ingredients.at(ingredient_index).original_string << "' rather than '" << caller_instruction.products.at(i).original_string << "'\n" << end();
          }
        }
      }
      finish_return_check:;
    }
  }
}

bool is_primitive(recipe_ordinal r) {
  return r < MAX_PRIMITIVE_RECIPES;
}

void test_return_type_mismatch() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  3:num <- f 2\n"
      "]\n"
      "def f [\n"
      "  12:num <- next-ingredient\n"
      "  13:num <- copy 35\n"
      "  14:point <- copy 12:point/raw\n"
      "  return 14:point\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: f: return ingredient '14:point' can't be saved in '3:num'\n"
  );
}

//: In Mu we'd like to assume that any instruction doesn't modify its
//: ingredients unless they're also products. The /same-as-ingredient inside
//: the recipe's 'return' indicates that an ingredient is intended to be
//: modified in place, and will help catch accidental misuse of such
//: 'ingredient-products' (sometimes called in-out parameters in other
//: languages).

void test_return_same_as_ingredient() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  1:num <- copy 0\n"
      "  2:num <- test1 1:num  # call with different ingredient and product\n"
      "]\n"
      "def test1 [\n"
      "  10:num <- next-ingredient\n"
      "  return 10:num/same-as-ingredient:0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: '2:num <- test1 1:num' should write to '1:num' rather than '2:num'\n"
  );
}

void test_return_same_as_ingredient_dummy() {
  run(
      "def main [\n"
      "  1:num <- copy 0\n"
      "  _ <- test1 1:num  # call with different ingredient and product\n"
      "]\n"
      "def test1 [\n"
      "  10:num <- next-ingredient\n"
      "  return 10:num/same-as-ingredient:0\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

string to_string(const vector<double>& in) {
  if (in.empty()) return "[]";
  ostringstream out;
  if (SIZE(in) == 1) {
    out << no_scientific(in.at(0));
    return out.str();
  }
  out << "[";
  for (int i = 0;  i < SIZE(in);  ++i) {
    if (i > 0) out << ", ";
    out << no_scientific(in.at(i));
  }
  out << "]";
  return out.str();
}
