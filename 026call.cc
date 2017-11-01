//: So far the recipes we define can't run each other. Let's fix that.

:(scenario calling_recipe)
def main [
  f
]
def f [
  3:num <- add 2, 2
]
+mem: storing 4 in location 3

:(scenario return_on_fallthrough)
def main [
  f
  1:num <- copy 0
  2:num <- copy 0
  3:num <- copy 0
]
def f [
  4:num <- copy 0
  5:num <- copy 0
]
+run: f
# running f
+run: {4: "number"} <- copy {0: "literal"}
+run: {5: "number"} <- copy {0: "literal"}
# back out to main
+run: {1: "number"} <- copy {0: "literal"}
+run: {2: "number"} <- copy {0: "literal"}
+run: {3: "number"} <- copy {0: "literal"}

:(before "struct routine {")
// Everytime a recipe runs another, we interrupt it and start running the new
// recipe. When that finishes, we continue this one where we left off.
// This requires maintaining a 'stack' of interrupted recipes or 'calls'.
struct call {
  recipe_ordinal running_recipe;
  int running_step_index;
  // End call Fields
  call(recipe_ordinal r) {
    running_recipe = r;
    running_step_index = 0;
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
  if (Trace_stream) {
    ++Trace_stream->callstack_depth;
    trace(9999, "trace") << "new routine; incrementing callstack depth to " << Trace_stream->callstack_depth << end();
    assert(Trace_stream->callstack_depth < 9000);  // 9998-101 plus cushion
  }
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
    if (Trace_stream) {
      ++Trace_stream->callstack_depth;
      trace(9999, "trace") << "incrementing callstack depth to " << Trace_stream->callstack_depth << end();
      assert(Trace_stream->callstack_depth < 9000);  // 9998-101 plus cushion
    }
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

:(scenario calling_undefined_recipe_fails)
% Hide_errors = true;
def main [
  foo
]
+error: main: undefined operation in 'foo'

:(scenario calling_undefined_recipe_handles_missing_result)
% Hide_errors = true;
def main [
  x:num <- foo
]
+error: main: undefined operation in 'x:num <- foo'

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
  if (Trace_stream) {
    trace(9999, "trace") << "fall-through: exiting " << current_recipe_name() << "; decrementing callstack depth from " << Trace_stream->callstack_depth << end();
    --Trace_stream->callstack_depth;
    assert(Trace_stream->callstack_depth >= 0);
  }
  Current_routine->calls.pop_front();
  if (Current_routine->calls.empty()) goto stop_running_current_routine;
  // Complete Call Fallthrough
  // todo: fail if no products returned
  ++current_step_index();
}
