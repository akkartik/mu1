//: Structured programming
//:
//: Our jump recipes are quite inconvenient to use, so Mu provides a
//: lightweight tool called 'transform_braces' to work in a slightly more
//: convenient format with nested braces:
//:
//:   {
//:     some instructions
//:     {
//:       more instructions
//:     }
//:   }
//:
//: Braces are just labels, they require no special parsing. The pseudo
//: instructions 'loop' and 'break' jump to just after the enclosing '{' and
//: '}' respectively.
//:
//: Conditional and unconditional 'loop' and 'break' should give us 80% of the
//: benefits of the control-flow primitives we're used to in other languages,
//: like 'if', 'while', 'for', etc.

:(scenarios transform)
:(scenario brace_conversion)
def main [
  {
    break
    1:num <- copy 0
  }
]
+transform: --- transform braces for recipe main
+transform: jump 1:offset
+transform: copy ...

:(before "End Instruction Modifying Transforms")
Transform.push_back(transform_braces);  // idempotent

:(code)
void transform_braces(const recipe_ordinal r) {
  const int OPEN = 0, CLOSE = 1;
  // use signed integer for step index because we'll be doing arithmetic on it
  list<pair<int/*OPEN/CLOSE*/, /*step*/int> > braces;
  trace(9991, "transform") << "--- transform braces for recipe " << get(Recipe, r).name << end();
  for (int index = 0;  index < SIZE(get(Recipe, r).steps);  ++index) {
    const instruction& inst = get(Recipe, r).steps.at(index);
    if (inst.label == "{") {
      trace(9993, "transform") << maybe(get(Recipe, r).name) << "push (open, " << index << ")" << end();
      braces.push_back(pair<int,int>(OPEN, index));
    }
    if (inst.label == "}") {
      trace(9993, "transform") << "push (close, " << index << ")" << end();
      braces.push_back(pair<int,int>(CLOSE, index));
    }
  }
  stack</*step*/int> open_braces;
  for (int index = 0;  index < SIZE(get(Recipe, r).steps);  ++index) {
    instruction& inst = get(Recipe, r).steps.at(index);
    if (inst.label == "{") {
      open_braces.push(index);
      continue;
    }
    if (inst.label == "}") {
      if (open_braces.empty()) {
        raise << maybe(get(Recipe, r).name) << "unbalanced '}'\n" << end();
        return;
      }
      open_braces.pop();
      continue;
    }
    if (inst.is_label) continue;
    if (inst.name != "loop"
         && inst.name != "loop-if"
         && inst.name != "loop-unless"
         && inst.name != "break"
         && inst.name != "break-if"
         && inst.name != "break-unless") {
      trace(9992, "transform") << inst.name << " ..." << end();
      continue;
    }
    // check for errors
    if (inst.name.find("-if") != string::npos || inst.name.find("-unless") != string::npos) {
      if (inst.ingredients.empty()) {
        raise << maybe(get(Recipe, r).name) << "'" << inst.name << "' expects 1 or 2 ingredients, but got none\n" << end();
        continue;
      }
    }
    // update instruction operation
    string old_name = inst.name;  // save a copy
    if (inst.name.find("-if") != string::npos) {
      inst.name = "jump-if";
      inst.operation = JUMP_IF;
    }
    else if (inst.name.find("-unless") != string::npos) {
      inst.name = "jump-unless";
      inst.operation = JUMP_UNLESS;
    }
    else {
      inst.name = "jump";
      inst.operation = JUMP;
    }
    // check for explicitly provided targets
    if (inst.name.find("-if") != string::npos || inst.name.find("-unless") != string::npos) {
      // conditional branches check arg 1
      if (SIZE(inst.ingredients) > 1 && is_literal(inst.ingredients.at(1))) {
        trace(9992, "transform") << inst.name << ' ' << inst.ingredients.at(1).name << ":offset" << end();
        continue;
      }
    }
    else {
      // unconditional branches check arg 0
      if (!inst.ingredients.empty() && is_literal(inst.ingredients.at(0))) {
        trace(9992, "transform") << "jump " << inst.ingredients.at(0).name << ":offset" << end();
        continue;
      }
    }
    // if implicit, compute target
    reagent target;
    target.type = new type_tree("offset");
    target.set_value(0);
    if (open_braces.empty())
      raise << maybe(get(Recipe, r).name) << "'" << old_name << "' needs a '{' before\n" << end();
    else if (old_name.find("loop") != string::npos)
      target.set_value(open_braces.top()-index);
    else  // break instruction
      target.set_value(matching_brace(open_braces.top(), braces, r) - index - 1);
    inst.ingredients.push_back(target);
    // log computed target
    if (inst.name == "jump")
      trace(9992, "transform") << "jump " << no_scientific(target.value) << ":offset" << end();
    else
      trace(9992, "transform") << inst.name << ' ' << inst.ingredients.at(0).name << ", " << no_scientific(target.value) << ":offset" << end();
  }
}

// returns a signed integer not just so that we can return -1 but also to
// enable future signed arithmetic
int matching_brace(int index, const list<pair<int, int> >& braces, recipe_ordinal r) {
  int stacksize = 0;
  for (list<pair<int, int> >::const_iterator p = braces.begin();  p != braces.end();  ++p) {
    if (p->second < index) continue;
    stacksize += (p->first ? 1 : -1);
    if (stacksize == 0) return p->second;
  }
  raise << maybe(get(Recipe, r).name) << "unbalanced '{'\n" << end();
  return SIZE(get(Recipe, r).steps);  // exit current routine
}

:(scenario loop)
def main [
  1:num <- copy 0
  2:num <- copy 0
  {
    3:num <- copy 0
    loop
  }
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: copy ...
+transform: copy ...
+transform: jump -2:offset

:(scenario break_empty_block)
def main [
  1:num <- copy 0
  {
    break
  }
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: jump 0:offset

:(scenario break_cascading)
def main [
  1:num <- copy 0
  {
    break
  }
  {
    break
  }
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: jump 0:offset
+transform: jump 0:offset

:(scenario break_cascading_2)
def main [
  1:num <- copy 0
  2:num <- copy 0
  {
    break
    3:num <- copy 0
  }
  {
    break
  }
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: copy ...
+transform: jump 1:offset
+transform: copy ...
+transform: jump 0:offset

:(scenario break_if)
def main [
  1:num <- copy 0
  2:num <- copy 0
  {
    break-if 2:num
    3:num <- copy 0
  }
  {
    break
  }
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: copy ...
+transform: jump-if 2, 1:offset
+transform: copy ...
+transform: jump 0:offset

:(scenario break_nested)
def main [
  1:num <- copy 0
  {
    2:num <- copy 0
    break
    {
      3:num <- copy 0
    }
    4:num <- copy 0
  }
]
+transform: jump 4:offset

:(scenario break_nested_degenerate)
def main [
  1:num <- copy 0
  {
    2:num <- copy 0
    break
    {
    }
    4:num <- copy 0
  }
]
+transform: jump 3:offset

:(scenario break_nested_degenerate_2)
def main [
  1:num <- copy 0
  {
    2:num <- copy 0
    break
    {
    }
  }
]
+transform: jump 2:offset

:(scenario break_label)
% Hide_errors = true;
def main [
  1:num <- copy 0
  {
    break +foo:offset
  }
]
+transform: jump +foo:offset

:(scenario break_unless)
def main [
  1:num <- copy 0
  2:num <- copy 0
  {
    break-unless 2:num
    3:num <- copy 0
  }
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: copy ...
+transform: jump-unless 2, 1:offset
+transform: copy ...

:(scenario loop_unless)
def main [
  1:num <- copy 0
  2:num <- copy 0
  {
    loop-unless 2:num
    3:num <- copy 0
  }
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: copy ...
+transform: jump-unless 2, -1:offset
+transform: copy ...

:(scenario loop_nested)
def main [
  1:num <- copy 0
  {
    2:num <- copy 0
    {
      3:num <- copy 0
    }
    loop-if 4:bool
    5:num <- copy 0
  }
]
+transform: --- transform braces for recipe main
+transform: jump-if 4, -5:offset

:(scenario loop_label)
def main [
  1:num <- copy 0
  +foo
  2:num <- copy 0
]
+transform: --- transform braces for recipe main
+transform: copy ...
+transform: copy ...

//: test how things actually run
:(scenarios run)
:(scenario brace_conversion_and_run)
def test-factorial [
  1:num <- copy 5
  2:num <- copy 1
  {
    3:bool <- equal 1:num, 1
    break-if 3:bool
#    $print 1:num
    2:num <- multiply 2:num, 1:num
    1:num <- subtract 1:num, 1
    loop
  }
  4:num <- copy 2:num  # trigger a read
]
+mem: location 2 is 120

:(scenario break_outside_braces_fails)
% Hide_errors = true;
def main [
  break
]
+error: main: 'break' needs a '{' before

:(scenario break_conditional_without_ingredient_fails)
% Hide_errors = true;
def main [
  {
    break-if
  }
]
+error: main: 'break-if' expects 1 or 2 ingredients, but got none

//: Using break we can now implement conditional returns.

:(scenario return_if)
def main [
  1:num <- test1
]
def test1 [
  return-if 0, 34
  return 35
]
+mem: storing 35 in location 1

:(scenario return_if_2)
def main [
  1:num <- test1
]
def test1 [
  return-if 1, 34
  return 35
]
+mem: storing 34 in location 1

:(before "End Rewrite Instruction(curr, recipe result)")
// rewrite 'return-if a, b, c, ...' to
//   ```
//   {
//     break-unless a
//     return b, c, ...
//   }
//   ```
if (curr.name == "return-if" || curr.name == "reply-if") {
  if (curr.products.empty()) {
    emit_return_block(result, "break-unless", curr);
    curr.clear();
  }
  else {
    raise << "'" << curr.name << "' never yields any products\n" << end();
  }
}
// rewrite 'return-unless a, b, c, ...' to
//   ```
//   {
//     break-if a
//     return b, c, ...
//   }
//   ```
if (curr.name == "return-unless" || curr.name == "reply-unless") {
  if (curr.products.empty()) {
    emit_return_block(result, "break-if", curr);
    curr.clear();
  }
  else {
    raise << "'" << curr.name << "' never yields any products\n" << end();
  }
}

:(code)
void emit_return_block(recipe& out, const string& break_command, const instruction& inst) {
  const vector<reagent>& ingredients = inst.ingredients;
  reagent/*copy*/ condition = ingredients.at(0);
  vector<reagent> return_ingredients;
  copy(++ingredients.begin(), ingredients.end(), inserter(return_ingredients, return_ingredients.end()));

  // {
  instruction open_label;  open_label.is_label=true;  open_label.label = "{";
  out.steps.push_back(open_label);

  // <break command> <condition>
  instruction break_inst;
  break_inst.operation = get(Recipe_ordinal, break_command);
  break_inst.name = break_command;
  break_inst.ingredients.push_back(condition);
  out.steps.push_back(break_inst);

  // return <return ingredients>
  instruction return_inst;
  return_inst.operation = get(Recipe_ordinal, "return");
  return_inst.name = "return";
  return_inst.ingredients.swap(return_ingredients);
  return_inst.original_string = inst.original_string;
  out.steps.push_back(return_inst);

  // }
  instruction close_label;  close_label.is_label=true;  close_label.label = "}";
  out.steps.push_back(close_label);
}

//: Make sure these pseudo recipes get consistent numbers in all tests, even
//: though they aren't implemented. Allows greater flexibility in ordering
//: transforms.

:(before "End Primitive Recipe Declarations")
BREAK,
BREAK_IF,
BREAK_UNLESS,
LOOP,
LOOP_IF,
LOOP_UNLESS,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "break", BREAK);
put(Recipe_ordinal, "break-if", BREAK_IF);
put(Recipe_ordinal, "break-unless", BREAK_UNLESS);
put(Recipe_ordinal, "loop", LOOP);
put(Recipe_ordinal, "loop-if", LOOP_IF);
put(Recipe_ordinal, "loop-unless", LOOP_UNLESS);
:(before "End Primitive Recipe Checks")
case BREAK: break;
case BREAK_IF: break;
case BREAK_UNLESS: break;
case LOOP: break;
case LOOP_IF: break;
case LOOP_UNLESS: break;
