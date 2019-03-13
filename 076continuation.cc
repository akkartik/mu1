//: Continuations are a powerful primitive for constructing advanced kinds of
//: control *policies* like back-tracking.
//:
//: In Mu, continuations are first-class and delimited, and are constructed
//: out of two primitives:
//:
//:  * 'call-with-continuation-mark' marks the top of the call stack and then
//:    calls the provided recipe.
//:  * 'return-continuation-until-mark' copies the top of the stack
//:    until the mark, and returns it as the result of
//:    'call-with-continuation-mark' (which might be a distant ancestor on the
//:    call stack; intervening calls don't return)
//:
//: The resulting slice of the stack can now be called just like a regular
//: recipe.
//:
//: See the example programs continuation*.mu to get a sense for the
//: possibilities.
//:
//: Refinements:
//:  * You can call a single continuation multiple times, and it will preserve
//:    the state of its local variables at each stack frame between calls.
//:    The stack frames of a continuation are not destroyed until the
//:    continuation goes out of scope. See continuation2.mu.
//:  * 'return-continuation-until-mark' doesn't consume the mark, so you can
//:    return multiple continuations based on a single mark. In combination
//:    with the fact that 'return-continuation-until-mark' can return from
//:    regular calls, just as long as there was an earlier call to
//:    'call-with-continuation-mark', this gives us a way to create resumable
//:    recipes. See continuation3.mu.
//:  * 'return-continuation-until-mark' can take ingredients to return just
//:    like other 'return' instructions. It just implicitly also returns a
//:    continuation as the first result. See continuation4.mu.
//:  * Conversely, you can pass ingredients to a continuation when calling it,
//:    to make it available to products of 'return-continuation-until-mark'.
//:    See continuation5.mu.
//:  * There can be multiple continuation marks on the stack at once;
//:    'call-with-continuation-mark' and 'return-continuation-until-mark' both
//:    need to pass in a tag to coordinate on the correct mark. This allows us
//:    to save multiple continuations for different purposes (say if one is
//:    for exceptions) with overlapping stack frames. See exception.mu.
//:
//: Inspired by James and Sabry, "Yield: Mainstream delimited continuations",
//: Workshop on the Theory and Practice of Delimited Continuations, 2011.
//: https://www.cs.indiana.edu/~sabry/papers/yield.pdf
//:
//: Caveats:
//:  * At the moment we can't statically type-check continuations. So we raise
//:    runtime errors for a call that doesn't return a continuation when the
//:    caller expects, or one that returns a continuation when the caller
//:    doesn't expect it. This shouldn't cause memory corruption, though.
//:    There should still be no way to lookup addresses that aren't allocated.

:(before "End Mu Types Initialization")
type_ordinal continuation = Type_ordinal["continuation"] = Next_type_ordinal++;
Type[continuation].name = "continuation";

//: A continuation can be called like a recipe.
:(before "End is_mu_recipe Atom Cases(r)")
if (r.type->name == "continuation") return true;

//: However, it can't be type-checked like most recipes. Pretend it's like a
//: header-less recipe.
:(after "Begin Reagent->Recipe(r, recipe_header)")
if (r.type->atom && r.type->name == "continuation") {
  result_header.has_header = false;
  return result_header;
}

:(code)
void test_delimited_continuation() {
  run(
      "recipe main [\n"
      "  1:continuation <- call-with-continuation-mark 233/mark, f, 77\n"  // 77 is an argument to f
      "  2:num <- copy 5\n"
      "  {\n"
      "    2:num <- call 1:continuation, 2:num\n"  // jump to 'return-continuation-until-mark' below
      "    3:bool <- greater-or-equal 2:num, 8\n"
      "    break-if 3:bool\n"
      "    loop\n"
      "  }\n"
      "]\n"
      "recipe f [\n"
      "  11:num <- next-ingredient\n"
      "  12:num <- g 11:num\n"
      "  return 12:num\n"
      "]\n"
      "recipe g [\n"
      "  21:num <- next-ingredient\n"
      "  22:num <- return-continuation-until-mark 233/mark\n"
      "  23:num <- add 22:num, 1\n"
      "  return 23:num\n"
      "]\n"
  );
  // first call of 'g' executes the part before return-continuation-until-mark
  CHECK_TRACE_CONTENTS(
      // first call of 'g' executes the part before return-continuation-until-mark
      "mem: storing 77 in location 21\n"
      "run: {2: \"number\"} <- copy {5: \"literal\"}\n"
      "mem: storing 5 in location 2\n"
      // calls of the continuation execute the part after return-continuation-until-mark
      "run: {2: \"number\"} <- call {1: \"continuation\"}, {2: \"number\"}\n"
      "mem: storing 5 in location 22\n"
      "mem: storing 6 in location 2\n"
      "run: {2: \"number\"} <- call {1: \"continuation\"}, {2: \"number\"}\n"
      "mem: storing 6 in location 22\n"
      "mem: storing 7 in location 2\n"
      "run: {2: \"number\"} <- call {1: \"continuation\"}, {2: \"number\"}\n"
      "mem: storing 7 in location 22\n"
      "mem: storing 8 in location 2\n"
  );
  // first call of 'g' does not execute the part after return-continuation-until-mark
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 77 in location 22");
  // calls of the continuation don't execute the part before return-continuation-until-mark
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 5 in location 21");
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 6 in location 21");
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 7 in location 21");
  // termination
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 9 in location 2");
}

:(before "End call Fields")
int continuation_mark_tag;
:(before "End call Constructor")
continuation_mark_tag = 0;

:(before "End Primitive Recipe Declarations")
CALL_WITH_CONTINUATION_MARK,
:(before "End Primitive Recipe Numbers")
Recipe_ordinal["call-with-continuation-mark"] = CALL_WITH_CONTINUATION_MARK;
:(before "End Primitive Recipe Checks")
case CALL_WITH_CONTINUATION_MARK: {
  if (SIZE(inst.ingredients) < 2) {
    raise << maybe(get(Recipe, r).name) << "'" << to_original_string(inst) << "' requires at least two ingredients: a mark number and a recipe to call\n" << end();
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case CALL_WITH_CONTINUATION_MARK: {
  // like call, but mark the current call as a 'base of continuation' call
  // before pushing the next one on it
  trace(Callstack_depth+1, "trace") << "delimited continuation; incrementing callstack depth to " << Callstack_depth << end();
  ++Callstack_depth;
  assert(Callstack_depth < Max_depth);
  instruction/*copy*/ caller_instruction = current_instruction();
  Current_routine->calls.front().continuation_mark_tag = current_instruction().ingredients.at(0).value;
  Current_routine->calls.push_front(call(ingredients.at(1).at(0)));
  // drop the mark
  caller_instruction.ingredients.erase(caller_instruction.ingredients.begin());
  ingredients.erase(ingredients.begin());
  // drop the callee
  caller_instruction.ingredients.erase(caller_instruction.ingredients.begin());
  ingredients.erase(ingredients.begin());
  finish_call_housekeeping(caller_instruction, ingredients);
  continue;
}

:(code)
void test_next_ingredient_inside_continuation() {
  run(
      "recipe main [\n"
      "  call-with-continuation-mark 233/mark, f, true\n"
      "]\n"
      "recipe f [\n"
      "  10:bool <- next-input\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 1 in location 10\n"
  );
}

void test_delimited_continuation_out_of_recipe_variable() {
  run(
      "recipe main [\n"
      "  x:recipe <- copy f\n"
      "  call-with-continuation-mark 233/mark, x, true\n"
      "]\n"
      "recipe f [\n"
      "  10:bool <- next-input\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 1 in location 10\n"
  );
}

//: save the slice of current call stack until the 'call-with-continuation-mark'
//: call, and return it as the result.
//: todo: implement delimited continuations in Mu's memory
:(before "End Types")
struct delimited_continuation {
  call_stack frames;
  int nrefs;
  delimited_continuation(call_stack::iterator begin, call_stack::iterator end) :frames(call_stack(begin, end)), nrefs(0) {}
};
:(before "End Globals")
map<long long int, delimited_continuation> Delimited_continuation;
long long int Next_delimited_continuation_id = 1;  // 0 is null just like an address
:(before "End Reset")
Delimited_continuation.clear();
Next_delimited_continuation_id = 1;

:(before "End Primitive Recipe Declarations")
RETURN_CONTINUATION_UNTIL_MARK,
:(before "End Primitive Recipe Numbers")
Recipe_ordinal["return-continuation-until-mark"] = RETURN_CONTINUATION_UNTIL_MARK;
:(before "End Primitive Recipe Checks")
case RETURN_CONTINUATION_UNTIL_MARK: {
  if (inst.ingredients.empty()) {
    raise << maybe(get(Recipe, r).name) << "'" << to_original_string(inst) << "' requires at least one ingredient: a mark tag (number)\n" << end();
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case RETURN_CONTINUATION_UNTIL_MARK: {
  // I don't know how to think about next-ingredient in combination with
  // continuations, so seems cleaner to just kill it. Functions have to read
  // their inputs before ever returning a continuation.
  Current_routine->calls.front().ingredient_atoms.clear();
  Current_routine->calls.front().next_ingredient_to_process = 0;
  // copy the current call stack until the most recent marked call
  call_stack::iterator find_base_of_continuation(call_stack&, int);  // manual prototype containing '::'
  call_stack::iterator base = find_base_of_continuation(Current_routine->calls, /*mark tag*/current_instruction().ingredients.at(0).value);
  if (base == Current_routine->calls.end()) {
    raise << maybe(current_recipe_name()) << "couldn't find a 'call-with-continuation-mark' to return to with tag " << current_instruction().ingredients.at(0).original_string << '\n' << end();
    raise << maybe(current_recipe_name()) << "call stack:\n" << end();
    for (call_stack::iterator p = Current_routine->calls.begin();  p != Current_routine->calls.end();  ++p)
      raise << maybe(current_recipe_name()) << "  " << get(Recipe, p->running_recipe).name << '\n' << end();
    break;
  }
  trace(Callstack_depth+1, "run") << "creating continuation " << Next_delimited_continuation_id << end();
  put(Delimited_continuation, Next_delimited_continuation_id, delimited_continuation(Current_routine->calls.begin(), base));
  while (Current_routine->calls.begin() != base) {
    --Callstack_depth;
    assert(Callstack_depth >= 0);
    Current_routine->calls.pop_front();
  }
  // return it as the result of the marked call
  products.resize(1);
  products.at(0).push_back(Next_delimited_continuation_id);
  // return any other ingredients passed in
  copy(/*skip mark tag*/++ingredients.begin(), ingredients.end(), inserter(products, products.end()));
  ++Next_delimited_continuation_id;
  break;  // continue to process rest of marked call
}

:(code)
call_stack::iterator find_base_of_continuation(call_stack& c, int mark_tag) {
  for (call_stack::iterator p = c.begin(); p != c.end(); ++p)
    if (p->continuation_mark_tag == mark_tag) return p;
  return c.end();
}

//: overload 'call' for continuations
:(after "Begin Call")
if (is_mu_continuation(current_instruction().ingredients.at(0))) {
  // copy multiple calls on to current call stack
  assert(scalar(ingredients.at(0)));
  trace(Callstack_depth+1, "run") << "calling continuation " << ingredients.at(0).at(0) << end();
  if (!contains_key(Delimited_continuation, ingredients.at(0).at(0)))
    raise << maybe(current_recipe_name()) << "no such delimited continuation " << current_instruction().ingredients.at(0).original_string << '\n' << end();
  const call_stack& new_frames = get(Delimited_continuation, ingredients.at(0).at(0)).frames;
  for (call_stack::const_reverse_iterator p = new_frames.rbegin(); p != new_frames.rend(); ++p)
    Current_routine->calls.push_front(*p);
  trace(Callstack_depth+1, "trace") << "calling delimited continuation; growing callstack depth to " << Callstack_depth+SIZE(new_frames) << end();
  Callstack_depth += SIZE(new_frames);
  assert(Callstack_depth < Max_depth);
  // no call housekeeping; continuations don't support next-ingredient
  copy(/*drop continuation*/++ingredients.begin(), ingredients.end(), inserter(products, products.begin()));
  break;  // record results of resuming 'return-continuation-until-mark' instruction
}

:(code)
void test_continuations_can_return_values() {
  run(
      "def main [\n"
      "  local-scope\n"
      "  k:continuation, 1:num/raw <- call-with-continuation-mark 233/mark, f\n"
      "]\n"
      "def f [\n"
      "  local-scope\n"
      "  g\n"
      "]\n"
      "def g [\n"
      "  local-scope\n"
      "  return-continuation-until-mark 233/mark, 34\n"
      "  stash [continuation called]\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 34 in location 1\n"
  );
}

void test_continuations_continue_to_matching_mark() {
  run(
      "def main [\n"
      "  local-scope\n"
      "  k:continuation, 1:num/raw <- call-with-continuation-mark 233/mark, f\n"
      "  add 1, 1\n"
      "]\n"
      "def f [\n"
      "  local-scope\n"
      "  k2:continuation <- call-with-continuation-mark 234/mark, g\n"
      "  add 2, 2\n"
      "]\n"
      "def g [\n"
      "  local-scope\n"
      "  return-continuation-until-mark 233/mark, 34\n"
      "  stash [continuation called]\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "run: add {1: \"literal\"}, {1: \"literal\"}\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("run: add {2: \"literal\"}, {2: \"literal\"}");
}

//: Allow shape-shifting recipes to return continuations.

void test_call_shape_shifting_recipe_with_continuation_mark() {
  run(
      "def main [\n"
      "  1:num <- call-with-continuation-mark 233/mark, f, 34\n"
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

:(before "End resolve_ambiguous_call(r, index, inst, caller_recipe) Special-cases")
if (inst.name == "call-with-continuation-mark") {
  if (SIZE(inst.ingredients) > 1 && is_recipe_literal(inst.ingredients.at(/*skip mark*/1))) {
  resolve_indirect_continuation_call(r, index, inst, caller_recipe);
  return;
  }
}
:(code)
void resolve_indirect_continuation_call(const recipe_ordinal r, int index, instruction& inst, const recipe& caller_recipe) {
  instruction inst2;
  inst2.name = inst.ingredients.at(/*skip mark*/1).name;
  for (int i = /*skip mark and recipe*/2;  i < SIZE(inst.ingredients);  ++i)
    inst2.ingredients.push_back(inst.ingredients.at(i));
  for (int i = /*skip continuation*/1;  i < SIZE(inst.products);  ++i)
    inst2.products.push_back(inst.products.at(i));
  resolve_ambiguous_call(r, index, inst2, caller_recipe);
  inst.ingredients.at(/*skip mark*/1).name = inst2.name;
  inst.ingredients.at(/*skip mark*/1).set_value(get(Recipe_ordinal, inst2.name));
}

void test_call_shape_shifting_recipe_with_continuation_mark_and_no_outputs() {
  run(
      "def main [\n"
      "  1:continuation <- call-with-continuation-mark 233/mark, f, 34\n"
      "]\n"
      "def f x:_elem [\n"
      "  local-scope\n"
      "  load-ingredients\n"
      "  return-continuation-until-mark 233/mark\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_continuation1() {
  run(
      "def main [\n"
      "  local-scope\n"
      "  k:continuation <- call-with-continuation-mark 233/mark, create-yielder\n"
      "  10:num/raw <- call k\n"
      "]\n"
      "def create-yielder -> n:num [\n"
      "  local-scope\n"
      "  load-inputs\n"
      "  return-continuation-until-mark 233/mark\n"
      "  return 1\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 1 in location 10\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

:(code)
bool is_mu_continuation(reagent/*copy*/ x) {
  canonize_type(x);
  return x.type && x.type->atom && x.type->value == get(Type_ordinal, "continuation");
}

// helper for debugging
void dump(const int continuation_id) {
  if (!contains_key(Delimited_continuation, continuation_id)) {
    raise << "missing delimited continuation: " << continuation_id << '\n' << end();
    return;
  }
  delimited_continuation& curr = get(Delimited_continuation, continuation_id);
  dump(curr.frames);
}
