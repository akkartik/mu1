//: So far the recipes we define can't run each other. Let's fix that.

void test_calling_recipe() {
  run(
      "def main [\n"
      "  f\n"
      "]\n"
      "def f [\n"
      "  3:num <- add 2, 2\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 4 in location 3\n"
  );
}

void test_return_on_fallthrough() {
  run(
      "def main [\n"
      "  f\n"
      "  1:num <- copy 0\n"
      "  2:num <- copy 0\n"
      "  3:num <- copy 0\n"
      "]\n"
      "def f [\n"
      "  4:num <- copy 0\n"
      "  5:num <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "run: f\n"
      "run: {4: \"number\"} <- copy {0: \"literal\"}\n"
      "run: {5: \"number\"} <- copy {0: \"literal\"}\n"
      "run: {1: \"number\"} <- copy {0: \"literal\"}\n"
      "run: {2: \"number\"} <- copy {0: \"literal\"}\n"
      "run: {3: \"number\"} <- copy {0: \"literal\"}\n"
  );
}

:(before "struct routine {")
// Everytime a recipe runs another, we interrupt it and start running the new
// recipe. When that finishes, we continue this one where we left off.
// This requires maintaining a 'stack' of interrupted recipes or 'calls'.
struct call {
  recipe_ordinal running_recipe;
  int running_step_index;
  // End call Fields
  call(recipe_ordinal r) { clear(r, 0); }
  call(recipe_ordinal r, int index) { clear(r, index); }
  void clear(recipe_ordinal r, int index) {
    running_recipe = r;
    running_step_index = index;
    // End call Constructor
  }
  ~call() {
    // End call Destructor
  }
};
typedef list<call> call_stack;

:(replace{} "struct routine")
struct routine {
  call_stack calls;
  // End routine Fields
  routine(recipe_ordinal r);
  bool completed() const;
  const vector<instruction>& steps() const;
};
:(code)
routine::routine(recipe_ordinal r) {
  ++Callstack_depth;
  trace(Callstack_depth+1, "trace") << "new routine; incrementing callstack depth to " << Callstack_depth << end();
  assert(Callstack_depth < Max_depth);
  calls.push_front(call(r));
  // End routine Constructor
}

//:: now update routine's helpers

//: macro versions for a slight speedup

:(delete{} "int& current_step_index()")
:(delete{} "recipe_ordinal currently_running_recipe()")
:(delete{} "const string& current_recipe_name()")
:(delete{} "const recipe& current_recipe()")
:(delete{} "const instruction& current_instruction()")

:(before "End Includes")
#define current_call() Current_routine->calls.front()
#define current_step_index() current_call().running_step_index
#define currently_running_recipe() current_call().running_recipe
#define current_recipe() get(Recipe, currently_running_recipe())
#define current_recipe_name() current_recipe().name
#define to_instruction(call) get(Recipe, (call).running_recipe).steps.at((call).running_step_index)
#define current_instruction() to_instruction(current_call())

//: function versions for debugging

:(code)
//? :(before "End Globals")
//? bool Foo2 = false;
//? :(code)
//? call& current_call() {
//?   if (Foo2) cerr << __FUNCTION__ << '\n';
//?   return Current_routine->calls.front();
//? }
//? :(replace{} "int& current_step_index()")
//? int& current_step_index() {
//?   assert(!Current_routine->calls.empty());
//?   if (Foo2) cerr << __FUNCTION__ << '\n';
//?   return current_call().running_step_index;
//? }
//? :(replace{} "recipe_ordinal currently_running_recipe()")
//? recipe_ordinal currently_running_recipe() {
//?   assert(!Current_routine->calls.empty());
//?   if (Foo2) cerr << __FUNCTION__ << '\n';
//?   return current_call().running_recipe;
//? }
//? :(replace{} "const string& current_recipe_name()")
//? const string& current_recipe_name() {
//?   assert(!Current_routine->calls.empty());
//?   if (Foo2) cerr << __FUNCTION__ << '\n';
//?   return get(Recipe, current_call().running_recipe).name;
//? }
//? :(replace{} "const recipe& current_recipe()")
//? const recipe& current_recipe() {
//?   assert(!Current_routine->calls.empty());
//?   if (Foo2) cerr << __FUNCTION__ << '\n';
//?   return get(Recipe, current_call().running_recipe);
//? }
//? :(replace{} "const instruction& current_instruction()")
//? const instruction& current_instruction() {
//?   assert(!Current_routine->calls.empty());
//?   if (Foo2) cerr << __FUNCTION__ << '\n';
//?   return to_instruction(current_call());
//? }
//? :(code)
//? const instruction& to_instruction(const call& call) {
//?   return get(Recipe, call.running_recipe).steps.at(call.running_step_index);
//? }

:(code)
void dump_callstack() {
  if (!Current_routine) return;
  if (Current_routine->calls.size() <= 1) return;
  for (call_stack::const_iterator p = ++Current_routine->calls.begin();  p != Current_routine->calls.end();  ++p)
    raise << "  called from " << get(Recipe, p->running_recipe).name << ": " << to_original_string(to_instruction(*p)) << '\n' << end();
}

:(after "Defined Recipe Checks")
// not a primitive; check that it's present in the book of recipes
if (!contains_key(Recipe, inst.operation)) {
  raise << maybe(get(Recipe, r).name) << "undefined operation in '" << to_original_string(inst) << "'\n" << end();
  break;
}
:(replace{} "default:" following "End Primitive Recipe Implementations")
default: {
  if (contains_key(Recipe, current_instruction().operation)) {  // error already raised in Checks above
    // not a primitive; look up the book of recipes
    ++Callstack_depth;
    trace(Callstack_depth+1, "trace") << "incrementing callstack depth to " << Callstack_depth << end();
    assert(Callstack_depth < Max_depth);
    const call& caller_frame = current_call();
    Current_routine->calls.push_front(call(to_instruction(caller_frame).operation));
    finish_call_housekeeping(to_instruction(caller_frame), ingredients);
    // not done with caller
    write_products = false;
    fall_through_to_next_instruction = false;
    // End Non-primitive Call(caller_frame)
  }
}
:(code)
void finish_call_housekeeping(const instruction& call_instruction, const vector<vector<double> >& ingredients) {
  // End Call Housekeeping
}

void test_calling_undefined_recipe_fails() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  foo\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: undefined operation in 'foo'\n"
  );
}

void test_calling_undefined_recipe_handles_missing_result() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  x:num <- foo\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: undefined operation in 'x:num <- foo'\n"
  );
}

//:: finally, we need to fix the termination conditions for the run loop

:(replace{} "bool routine::completed() const")
bool routine::completed() const {
  return calls.empty();
}

:(replace{} "const vector<instruction>& routine::steps() const")
const vector<instruction>& routine::steps() const {
  assert(!calls.empty());
  return get(Recipe, calls.front().running_recipe).steps;
}

:(after "Running One Instruction")
// when we reach the end of one call, we may reach the end of the one below
// it, and the one below that, and so on
while (current_step_index() >= SIZE(Current_routine->steps())) {
  // Falling Through End Of Recipe
  trace(Callstack_depth+1, "trace") << "fall-through: exiting " << current_recipe_name() << "; decrementing callstack depth from " << Callstack_depth << end();
  --Callstack_depth;
  assert(Callstack_depth >= 0);
  Current_routine->calls.pop_front();
  if (Current_routine->calls.empty()) goto stop_running_current_routine;
  // Complete Call Fallthrough
  // todo: fail if no products returned
  ++current_step_index();
}

:(before "End Primitive Recipe Declarations")
_DUMP_CALL_STACK,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$dump-call-stack", _DUMP_CALL_STACK);
:(before "End Primitive Recipe Checks")
case _DUMP_CALL_STACK: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _DUMP_CALL_STACK: {
  dump(Current_routine->calls);
  break;
}
:(code)
void dump(const call_stack& calls) {
  for (call_stack::const_reverse_iterator p = calls.rbegin(); p != calls.rend(); ++p)
    cerr << get(Recipe, p->running_recipe).name << ":" << p->running_step_index << " -- " << to_string(to_instruction(*p)) << '\n';
}
