//: Allow Mu programs to log facts just like we've been doing in C++ so far.

void test_trace() {
  run(
      "def main [\n"
      "  trace 1, [foo], [this is a trace in Mu]\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "foo: this is a trace in Mu\n"
  );
}

:(before "End Primitive Recipe Declarations")
TRACE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "trace", TRACE);
:(before "End Primitive Recipe Checks")
case TRACE: {
  if (SIZE(inst.ingredients) < 3) {
    raise << maybe(get(Recipe, r).name) << "'trace' takes three or more ingredients rather than '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!is_mu_number(inst.ingredients.at(0))) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'trace' should be a number (depth), but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  if (!is_literal_text(inst.ingredients.at(1))) {
    raise << maybe(get(Recipe, r).name) << "second ingredient of 'trace' should be a literal string (label), but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case TRACE: {
  int depth = ingredients.at(0).at(0);
  string label = current_instruction().ingredients.at(1).name;
  ostringstream out;
  for (int i = 2;  i < SIZE(current_instruction().ingredients);  ++i) {
    if (i > 2) out << ' ';
    out << inspect(current_instruction().ingredients.at(i), ingredients.at(i));
  }
  trace(depth, label) << out.str() << end();
  break;
}

//: simpler limited version of 'trace'

:(before "End Types")  //: include in all cleaved compilation units
const int App_depth = 1;  // where all Mu code will trace to by default

:(before "End Primitive Recipe Declarations")
STASH,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "stash", STASH);
:(before "End Primitive Recipe Checks")
case STASH: {
  break;
}
:(before "End Primitive Recipe Implementations")
case STASH: {
  ostringstream out;
  for (int i = 0;  i < SIZE(current_instruction().ingredients);  ++i) {
    if (i) out << ' ';
    out << inspect(current_instruction().ingredients.at(i), ingredients.at(i));
  }
  trace(App_depth, "app") << out.str() << end();
  break;
}

:(code)
void test_stash_literal_string() {
  run(
      "def main [\n"
      "  stash [foo]\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "app: foo\n"
  );
}

void test_stash_literal_number() {
  run(
      "def main [\n"
      "  stash [foo:], 4\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "app: foo: 4\n"
  );
}

void test_stash_number() {
  run(
      "def main [\n"
      "  1:num <- copy 34\n"
      "  stash [foo:], 1:num\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "app: foo: 34\n"
  );
}

:(code)
string inspect(const reagent& r, const vector<double>& data) {
  if (is_literal(r))
    return r.name;
  // End inspect Special-cases(r, data)
  ostringstream out;
  for (long long i = 0;  i < SIZE(data);  ++i) {
    if (i) out << ' ';
    out << no_scientific(data.at(i));
  }
  return out.str();
}

:(before "End Primitive Recipe Declarations")
HIDE_ERRORS,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "hide-errors", HIDE_ERRORS);
:(before "End Primitive Recipe Checks")
case HIDE_ERRORS: {
  break;
}
:(before "End Primitive Recipe Implementations")
case HIDE_ERRORS: {
  Hide_errors = true;
  break;
}

:(before "End Primitive Recipe Declarations")
SHOW_ERRORS,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "show-errors", SHOW_ERRORS);
:(before "End Primitive Recipe Checks")
case SHOW_ERRORS: {
  break;
}
:(before "End Primitive Recipe Implementations")
case SHOW_ERRORS: {
  Hide_errors = false;
  break;
}

:(before "End Primitive Recipe Declarations")
TRACE_UNTIL,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "trace-until", TRACE_UNTIL);
:(before "End Primitive Recipe Checks")
case TRACE_UNTIL: {
  break;
}
:(before "End Primitive Recipe Implementations")
case TRACE_UNTIL: {
  if (Trace_stream) {
    Trace_stream->collect_depth = ingredients.at(0).at(0);
  }
  break;
}

:(before "End Primitive Recipe Declarations")
_DUMP_TRACE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$dump-trace", _DUMP_TRACE);
:(before "End Primitive Recipe Checks")
case _DUMP_TRACE: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _DUMP_TRACE: {
  if (ingredients.empty()) {
    DUMP("");
  }
  else {
    DUMP(current_instruction().ingredients.at(0).name);
  }
  break;
}

:(before "End Primitive Recipe Declarations")
_CLEAR_TRACE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$clear-trace", _CLEAR_TRACE);
:(before "End Primitive Recipe Checks")
case _CLEAR_TRACE: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _CLEAR_TRACE: {
  if (Trace_stream) Trace_stream->past_lines.clear();
  break;
}

//:: 'cheating' by using the host system

:(before "End Primitive Recipe Declarations")
_PRINT,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$print", _PRINT);
:(before "End Primitive Recipe Checks")
case _PRINT: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _PRINT: {
  for (int i = 0;  i < SIZE(ingredients);  ++i) {
    if (is_literal(current_instruction().ingredients.at(i))) {
      trace(Callstack_depth+1, "run") << "$print: " << current_instruction().ingredients.at(i).name << end();
      if (!has_property(current_instruction().ingredients.at(i), "newline")) {
        cout << current_instruction().ingredients.at(i).name;
      }
      // hack: '$print 10' prints '10', but '$print 10/newline' prints '\n'
      // End $print 10/newline Special-cases
      else {
        cout << '\n';
      }
    }
    // End $print Special-cases
    else {
      for (int j = 0;  j < SIZE(ingredients.at(i));  ++j) {
        trace(Callstack_depth+1, "run") << "$print: " << ingredients.at(i).at(j) << end();
        if (j > 0) cout << " ";
        cout << no_scientific(ingredients.at(i).at(j));
      }
    }
  }
  cout.flush();
  break;
}

:(before "End Primitive Recipe Declarations")
_EXIT,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$exit", _EXIT);
:(before "End Primitive Recipe Checks")
case _EXIT: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _EXIT: {
  exit(0);
  break;
}

:(before "End Primitive Recipe Declarations")
_SYSTEM,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$system", _SYSTEM);
:(before "End Primitive Recipe Checks")
case _SYSTEM: {
  if (SIZE(inst.ingredients) != 1) {
    raise << maybe(get(Recipe, r).name) << "'$system' requires exactly one ingredient, but got '" << to_string(inst) << "'\n" << end();
    break;
  }
  if (!is_literal_text(inst.ingredients.at(0))) {
    raise << maybe(get(Recipe, r).name) << "ingredient to '$system' must be a literal text, but got '" << to_string(inst) << "'\n" << end();
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case _SYSTEM: {
  int status = system(current_instruction().ingredients.at(0).name.c_str());
  products.resize(1);
  products.at(0).push_back(status);
  break;
}

:(before "End Primitive Recipe Declarations")
_DUMP_MEMORY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$dump-memory", _DUMP_MEMORY);
:(before "End Primitive Recipe Checks")
case _DUMP_MEMORY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _DUMP_MEMORY: {
  dump_memory();
  break;
}

//: In times of real extremis we need to create a whole new modality for debug
//: logs, independent of other changes to the screen or Trace_stream.

:(before "End Globals")
ofstream LOG;
:(before "End One-time Setup")
//? LOG.open("log");

:(before "End Primitive Recipe Declarations")
_LOG,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$log", _LOG);
:(before "End Primitive Recipe Checks")
case _LOG: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _LOG: {
  ostringstream out;
  for (int i = 0;  i < SIZE(current_instruction().ingredients);  ++i) {
    out << inspect(current_instruction().ingredients.at(i), ingredients.at(i));
  }
  LOG << out.str() << '\n';
  break;
}

//: set a variable from within Mu code
//: useful for selectively tracing or printing after some point
:(before "End Globals")
bool Foo = false;
:(before "End Primitive Recipe Declarations")
_FOO,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$foo", _FOO);
:(before "End Primitive Recipe Checks")
case _FOO: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _FOO: {
  Foo = true;
  break;
}
