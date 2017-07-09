//: Allow code for recipes to be pulled in from multiple places and inserted
//: at special labels called 'waypoints' using two new top-level commands:
//:   before
//:   after

//: Most labels are local: they must be unique to a recipe, and are invisible
//: outside the recipe. However, waypoints are global: a recipe can have
//: multiple of them, you can't use them as jump targets.
:(before "End is_jump_target Special-cases")
if (is_waypoint(label)) return false;
//: Waypoints are always surrounded by '<>', e.g. <handle-request>.
:(code)
bool is_waypoint(string label) {
  return *label.begin() == '<' && *label.rbegin() == '>';
}

:(scenario tangle_before)
def main [
  1:num <- copy 0
  <label1>
  3:num <- copy 0
]
before <label1> [
  2:num <- copy 0
]
+mem: storing 0 in location 1
+mem: storing 0 in location 2
+mem: storing 0 in location 3
# nothing else
$mem: 3

//: while loading recipes, load before/after fragments

:(before "End Globals")
map<string /*label*/, recipe> Before_fragments, After_fragments;
set<string /*label*/> Fragments_used;
:(before "End Reset")
Before_fragments.clear();
After_fragments.clear();
Fragments_used.clear();

:(before "End Command Handlers")
else if (command == "before") {
  string label = next_word(in);
  if (label.empty()) {
    assert(!has_data(in));
    raise << "incomplete 'before' block at end of file\n" << end();
    return result;
  }
  recipe tmp;
  slurp_body(in, tmp);
  if (is_waypoint(label))
    Before_fragments[label].steps.insert(Before_fragments[label].steps.end(), tmp.steps.begin(), tmp.steps.end());
  else
    raise << "can't tangle before non-waypoint " << label << '\n' << end();
  // End before Command Handler
}
else if (command == "after") {
  string label = next_word(in);
  if (label.empty()) {
    assert(!has_data(in));
    raise << "incomplete 'after' block at end of file\n" << end();
    return result;
  }
  recipe tmp;
  slurp_body(in, tmp);
  if (is_waypoint(label))
    After_fragments[label].steps.insert(After_fragments[label].steps.begin(), tmp.steps.begin(), tmp.steps.end());
  else
    raise << "can't tangle after non-waypoint " << label << '\n' << end();
  // End after Command Handler
}

//: after all recipes are loaded, insert fragments at appropriate labels.

:(after "Begin Instruction Inserting/Deleting Transforms")
Transform.push_back(insert_fragments);  // NOT idempotent

//: We might need to perform multiple passes, in case inserted fragments
//: include more labels that need further insertions. Track which labels we've
//: already processed using an extra field.
:(before "End instruction Fields")
mutable bool tangle_done;
:(before "End instruction Constructor")
tangle_done = false;

:(code)
void insert_fragments(const recipe_ordinal r) {
  insert_fragments(get(Recipe, r));
}

void insert_fragments(recipe& r) {
  trace(9991, "transform") << "--- insert fragments into recipe " << r.name << end();
  bool made_progress = true;
  int pass = 0;
  while (made_progress) {
    made_progress = false;
    // create a new vector because insertions invalidate iterators
    vector<instruction> result;
    for (int i = 0;  i < SIZE(r.steps);  ++i) {
      const instruction& inst = r.steps.at(i);
      if (!inst.is_label || !is_waypoint(inst.label) || inst.tangle_done) {
        result.push_back(inst);
        continue;
      }
      inst.tangle_done = true;
      made_progress = true;
      Fragments_used.insert(inst.label);
      ostringstream prefix;
      prefix << '+' << r.name << '_' << pass << '_' << i;
      // ok to use contains_key even though Before_fragments uses [],
      // because appending an empty recipe is a noop
      if (contains_key(Before_fragments, inst.label)) {
        trace(9992, "transform") << "insert fragments before label " << inst.label << end();
        append_fragment(result, Before_fragments[inst.label].steps, prefix.str());
      }
      result.push_back(inst);
      if (contains_key(After_fragments, inst.label)) {
        trace(9992, "transform") << "insert fragments after label " << inst.label << end();
        append_fragment(result, After_fragments[inst.label].steps, prefix.str());
      }
    }
    r.steps.swap(result);
    ++pass;
  }
}

void append_fragment(vector<instruction>& base, const vector<instruction>& patch, const string prefix) {
  // append 'patch' to 'base' while keeping 'base' oblivious to any new jump
  // targets in 'patch' oblivious to 'base' by prepending 'prefix' to them.
  // we might tangle the same fragment at multiple points in a single recipe,
  // and we need to avoid duplicate jump targets.
  // so we'll keep jump targets local to the specific before/after fragment
  // that introduces them.
  set<string> jump_targets;
  for (int i = 0;  i < SIZE(patch);  ++i) {
    const instruction& inst = patch.at(i);
    if (inst.is_label && is_jump_target(inst.label))
      jump_targets.insert(inst.label);
  }
  for (int i = 0;  i < SIZE(patch);  ++i) {
    instruction inst = patch.at(i);
    if (inst.is_label) {
      if (contains_key(jump_targets, inst.label))
        inst.label = prefix+inst.label;
      base.push_back(inst);
      continue;
    }
    for (int j = 0;  j < SIZE(inst.ingredients);  ++j) {
      reagent& x = inst.ingredients.at(j);
      if (is_jump_target(x.name) && contains_key(jump_targets, x.name))
        x.name = prefix+x.name;
    }
    base.push_back(inst);
  }
}

//: complain about unapplied fragments
//: This can't run during transform because later (shape-shifting recipes)
//: we'll encounter situations where fragments might get used long after
//: they're loaded, and we might run transform_all in between. To avoid
//: spurious errors, run this check right at the end, after all code is
//: loaded, right before we run main.
:(before "End Commandline Parsing")
check_insert_fragments();
:(code)
void check_insert_fragments() {
  for (map<string, recipe>::iterator p = Before_fragments.begin();  p != Before_fragments.end();  ++p) {
    if (!contains_key(Fragments_used, p->first))
      raise << "could not locate insert before label " << p->first << '\n' << end();
  }
  for (map<string, recipe>::iterator p = After_fragments.begin();  p != After_fragments.end();  ++p) {
    if (!contains_key(Fragments_used, p->first))
      raise << "could not locate insert after label " << p->first << '\n' << end();
  }
}

:(scenario tangle_before_and_after)
def main [
  1:num <- copy 0
  <label1>
  4:num <- copy 0
]
before <label1> [
  2:num <- copy 0
]
after <label1> [
  3:num <- copy 0
]
+mem: storing 0 in location 1
+mem: storing 0 in location 2
# label1
+mem: storing 0 in location 3
+mem: storing 0 in location 4
# nothing else
$mem: 4

:(scenario tangle_ignores_jump_target)
% Hide_errors = true;
def main [
  1:num <- copy 0
  +label1
  4:num <- copy 0
]
before +label1 [
  2:num <- copy 0
]
+error: can't tangle before non-waypoint +label1

:(scenario tangle_keeps_labels_separate)
def main [
  1:num <- copy 0
  <label1>
  <label2>
  6:num <- copy 0
]
before <label1> [
  2:num <- copy 0
]
after <label1> [
  3:num <- copy 0
]
before <label2> [
  4:num <- copy 0
]
after <label2> [
  5:num <- copy 0
]
+mem: storing 0 in location 1
+mem: storing 0 in location 2
# label1
+mem: storing 0 in location 3
# 'after' fragments for earlier label always go before 'before' fragments for later label
+mem: storing 0 in location 4
# label2
+mem: storing 0 in location 5
+mem: storing 0 in location 6
# nothing else
$mem: 6

:(scenario tangle_stacks_multiple_fragments)
def main [
  1:num <- copy 0
  <label1>
  6:num <- copy 0
]
before <label1> [
  2:num <- copy 0
]
after <label1> [
  3:num <- copy 0
]
before <label1> [
  4:num <- copy 0
]
after <label1> [
  5:num <- copy 0
]
+mem: storing 0 in location 1
# 'before' fragments stack in order
+mem: storing 0 in location 2
+mem: storing 0 in location 4
# label1
# 'after' fragments stack in reverse order
+mem: storing 0 in location 5
+mem: storing 0 in location 3
+mem: storing 0 in location 6
# nothing else
$mem: 6

:(scenario tangle_supports_fragments_with_multiple_instructions)
def main [
  1:num <- copy 0
  <label1>
  6:num <- copy 0
]
before <label1> [
  2:num <- copy 0
  3:num <- copy 0
]
after <label1> [
  4:num <- copy 0
  5:num <- copy 0
]
+mem: storing 0 in location 1
+mem: storing 0 in location 2
+mem: storing 0 in location 3
# label1
+mem: storing 0 in location 4
+mem: storing 0 in location 5
+mem: storing 0 in location 6
# nothing else
$mem: 6

:(scenario tangle_tangles_into_all_labels_with_same_name)
def main [
  1:num <- copy 10
  <label1>
  4:num <- copy 10
  recipe2
]
def recipe2 [
  1:num <- copy 11
  <label1>
  4:num <- copy 11
]
before <label1> [
  2:num <- copy 12
]
after <label1> [
  3:num <- copy 12
]
+mem: storing 10 in location 1
+mem: storing 12 in location 2
# label1
+mem: storing 12 in location 3
+mem: storing 10 in location 4
# recipe2
+mem: storing 11 in location 1
+mem: storing 12 in location 2
# label1
+mem: storing 12 in location 3
+mem: storing 11 in location 4
# nothing else
$mem: 8

:(scenario tangle_tangles_into_all_labels_with_same_name_2)
def main [
  1:num <- copy 10
  <label1>
  <label1>
  4:num <- copy 10
]
before <label1> [
  2:num <- copy 12
]
after <label1> [
  3:num <- copy 12
]
+mem: storing 10 in location 1
+mem: storing 12 in location 2
# label1
+mem: storing 12 in location 3
+mem: storing 12 in location 2
# label1
+mem: storing 12 in location 3
+mem: storing 10 in location 4
# nothing else
$mem: 6

:(scenario tangle_tangles_into_all_labels_with_same_name_3)
def main [
  1:num <- copy 10
  <label1>
  <foo>
  4:num <- copy 10
]
before <label1> [
  2:num <- copy 12
]
after <label1> [
  3:num <- copy 12
]
after <foo> [
  <label1>
]
+mem: storing 10 in location 1
+mem: storing 12 in location 2
# label1
+mem: storing 12 in location 3
+mem: storing 12 in location 2
# foo/label1
+mem: storing 12 in location 3
+mem: storing 10 in location 4
# nothing else
$mem: 6

:(scenario tangle_handles_jump_target_inside_fragment)
def main [
  1:num <- copy 10
  <label1>
  4:num <- copy 10
]
before <label1> [
  jump +label2:label
  2:num <- copy 12
  +label2
  3:num <- copy 12
]
+mem: storing 10 in location 1
# label1
+mem: storing 12 in location 3
+mem: storing 10 in location 4
# ignored by jump
-mem: storing 12 in label 2
# nothing else
$mem: 3

:(scenario tangle_renames_jump_target)
def main [
  1:num <- copy 10
  <label1>
  +label2
  4:num <- copy 10
]
before <label1> [
  jump +label2:label
  2:num <- copy 12
  +label2  # renamed
  3:num <- copy 12
]
+mem: storing 10 in location 1
# label1
+mem: storing 12 in location 3
+mem: storing 10 in location 4
# ignored by jump
-mem: storing 12 in label 2
# nothing else
$mem: 3

:(scenario tangle_jump_to_base_recipe)
def main [
  1:num <- copy 10
  <label1>
  +label2
  4:num <- copy 10
]
before <label1> [
  jump +label2:label
  2:num <- copy 12
  3:num <- copy 12
]
+mem: storing 10 in location 1
# label1
+mem: storing 10 in location 4
# ignored by jump
-mem: storing 12 in label 2
-mem: storing 12 in location 3
# nothing else
$mem: 2

//: ensure that there are no new fragments created for a label after it's already been inserted to

:(code)
void test_new_fragment_after_tangle() {
  // define a recipe
  load("def foo [\n"
       "  local-scope\n"
       "  <label>\n"
       "]\n"
       "after <label> [\n"
       "  1:num/raw <- copy 34\n"
       "]\n");
  transform_all();
  CHECK_TRACE_DOESNT_CONTAIN_ERRORS();
  Hide_errors = true;
  // try to tangle into recipe foo after transform
  load("before <label> [\n"
       "  2:num/raw <- copy 35\n"
       "]\n");
  CHECK_TRACE_CONTAINS_ERRORS();
}

:(before "End before Command Handler")
if (contains_key(Fragments_used, label))
  raise << "we've already tangled some code at label " << label << " in a previous call to transform_all(). Those locations won't be updated.\n" << end();
:(before "End after Command Handler")
if (contains_key(Fragments_used, label))
  raise << "we've already tangled some code at label " << label << " in a previous call to transform_all(). Those locations won't be updated.\n" << end();
