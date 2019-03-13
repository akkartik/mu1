//: Support jumps to a specific label (the 'jump target') in the same recipe.
//: Jump targets must be unique and unambiguous within any recipe.
//:
//: The 'break' and 'loop' pseudo instructions can also take jump targets.
//: Which instruction you use is just documentation about intent; use 'break'
//: to indicate you're exiting one or more loop nests, and 'loop' to indicate
//: you're skipping to the next iteration of some containing loop nest.

//: Since they have to be unique in a recipe, not all labels can be jump
//: targets.
bool is_jump_target(const string& label) {
  if (label == "{" || label == "}") return false;
  // End is_jump_target Special-cases
  return is_label_word(label);
}

void test_jump_to_label() {
  run(
      "def main [\n"
      "  jump +target:label\n"
      "  1:num <- copy 0\n"
      "  +target\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 0 in location 1");
}

:(before "End Mu Types Initialization")
put(Type_ordinal, "label", 0);

:(before "End Instruction Modifying Transforms")
Transform.push_back(transform_labels);  // idempotent

:(code)
void transform_labels(const recipe_ordinal r) {
  map<string, int> offset;
  for (int i = 0;  i < SIZE(get(Recipe, r).steps);  ++i) {
    const instruction& inst = get(Recipe, r).steps.at(i);
    if (!inst.is_label) continue;
    if (is_jump_target(inst.label)) {
      if (!contains_key(offset, inst.label)) {
        put(offset, inst.label, i);
      }
      else {
        raise << maybe(get(Recipe, r).name) << "duplicate label '" << inst.label << "'" << end();
        // have all jumps skip some random but noticeable and deterministic amount of code
        put(offset, inst.label, 9999);
      }
    }
  }
  for (int i = 0;  i < SIZE(get(Recipe, r).steps);  ++i) {
    instruction& inst = get(Recipe, r).steps.at(i);
    if (inst.name == "jump") {
      if (inst.ingredients.empty()) {
        raise << maybe(get(Recipe, r).name) << "'" << to_original_string(inst) << "' expects an ingredient but got 0\n" << end();
        return;
      }
      replace_offset(inst.ingredients.at(0), offset, i, r);
    }
    if (inst.name == "jump-if" || inst.name == "jump-unless") {
      if (SIZE(inst.ingredients) < 2) {
        raise << maybe(get(Recipe, r).name) << "'" << to_original_string(inst) << "' expects 2 ingredients but got " << SIZE(inst.ingredients) << '\n' << end();
        return;
      }
      replace_offset(inst.ingredients.at(1), offset, i, r);
    }
    if ((inst.name == "loop" || inst.name == "break")
        && SIZE(inst.ingredients) >= 1) {
      replace_offset(inst.ingredients.at(0), offset, i, r);
    }
    if ((inst.name == "loop-if" || inst.name == "loop-unless"
            || inst.name == "break-if" || inst.name == "break-unless")
        && SIZE(inst.ingredients) >= 2) {
      replace_offset(inst.ingredients.at(1), offset, i, r);
    }
  }
}

void replace_offset(reagent& x, /*const*/ map<string, int>& offset, const int current_offset, const recipe_ordinal r) {
  if (!is_literal(x)) {
    raise << maybe(get(Recipe, r).name) << "jump target must be offset or label but is '" << x.original_string << "'\n" << end();
    x.set_value(0);  // no jump by default
    return;
  }
  if (x.initialized) return;
  if (is_integer(x.name)) return;  // non-labels will be handled like other number operands
  if (!is_jump_target(x.name)) {
    raise << maybe(get(Recipe, r).name) << "can't jump to label '" << x.name << "'\n" << end();
    x.set_value(0);  // no jump by default
    return;
  }
  if (!contains_key(offset, x.name)) {
    raise << maybe(get(Recipe, r).name) << "can't find label '" << x.name << "'\n" << end();
    x.set_value(0);  // no jump by default
    return;
  }
  x.set_value(get(offset, x.name) - current_offset);
}

void test_break_to_label() {
  run(
      "def main [\n"
      "  {\n"
      "    {\n"
      "      break +target:label\n"
      "      1:num <- copy 0\n"
      "    }\n"
      "  }\n"
      "  +target\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 0 in location 1");
}

void test_jump_if_to_label() {
  run(
      "def main [\n"
      "  {\n"
      "    {\n"
      "      jump-if 1, +target:label\n"
      "      1:num <- copy 0\n"
      "    }\n"
      "  }\n"
      "  +target\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 0 in location 1");
}

void test_loop_unless_to_label() {
  run(
      "def main [\n"
      "  {\n"
      "    {\n"
      "      loop-unless 0, +target:label\n"  // loop/break with a label don't care about braces
      "      1:num <- copy 0\n"
      "    }\n"
      "  }\n"
      "  +target\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 0 in location 1");
}

void test_jump_runs_code_after_label() {
  run(
      "def main [\n"
      // first a few lines of padding to exercise the offset computation
      "  1:num <- copy 0\n"
      "  2:num <- copy 0\n"
      "  3:num <- copy 0\n"
      "  jump +target:label\n"
      "  4:num <- copy 0\n"
      "  +target\n"
      "  5:num <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "mem: storing 0 in location 5\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("mem: storing 0 in location 4");
}

void test_jump_fails_without_target() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  jump\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: 'jump' expects an ingredient but got 0\n"
  );
}

void test_jump_fails_without_target_2() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  jump-if true\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: 'jump-if true' expects 2 ingredients but got 1\n"
  );
}

void test_recipe_fails_on_duplicate_jump_target() {
  Hide_errors = true;
  run(
      "def main [\n"
      "  +label\n"
      "  1:num <- copy 0\n"
      "  +label\n"
      "  2:num <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: duplicate label '+label'\n"
  );
}

void test_jump_ignores_nontarget_label() {
  Hide_errors = true;
  run(
      "def main [\n"
      // first a few lines of padding to exercise the offset computation
      "  1:num <- copy 0\n"
      "  2:num <- copy 0\n"
      "  3:num <- copy 0\n"
      "  jump $target:label\n"
      "  4:num <- copy 0\n"
      "  $target\n"
      "  5:num <- copy 0\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: main: can't jump to label '$target'\n"
  );
}
