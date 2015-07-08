//: Mu scenarios. This will get long, but these are the tests we want to
//: support in this layer.

//: You can use variable names in scenarios, but for the most part we'll use
//: raw location numbers, because that lets us make assertions on memory.
//: Tests should avoid abstraction as far as possible.
:(scenarios run_mu_scenario)
:(scenario scenario_block)
scenario foo [
  run [
    1:number <- copy 13:literal
  ]
  memory-should-contain [
    1 <- 13
  ]
]
# checks are inside scenario

:(scenario scenario_multiple_blocks)
scenario foo [
  run [
    1:number <- copy 13:literal
  ]
  memory-should-contain [
    1 <- 13
  ]
  run [
    2:number <- copy 13:literal
  ]
  memory-should-contain [
    1 <- 13
    2 <- 13
  ]
]

:(scenario scenario_check_memory_and_trace)
scenario foo [
  run [
    1:number <- copy 13:literal
    trace [a], [a b c]
  ]
  memory-should-contain [
    1 <- 13
  ]
  trace-should-contain [
    a: a b c
  ]
  trace-should-not-contain [
    a: x y z
  ]
]

//:: Core data structure

:(before "End Types")
struct scenario {
  string name;
  string to_run;
};

:(before "End Globals")
vector<scenario> Scenarios;

//:: Parse the 'scenario' form.
//: Simply store the text of the scenario.

:(before "End Command Handlers")
else if (command == "scenario") {
  Scenarios.push_back(parse_scenario(in));
}

:(code)
scenario parse_scenario(istream& in) {
//?   cerr << "parse scenario\n"; //? 1
  scenario result;
  result.name = next_word(in);
//?   cerr << "scenario: " << result.name << '\n'; //? 2
  skip_whitespace_and_comments(in);
  assert(in.peek() == '[');
  // scenarios are take special 'code' strings so we need to ignore brackets
  // inside comments
  result.to_run = slurp_quoted(in);
  // delete [] delimiters
  assert(result.to_run.at(0) == '[');
  result.to_run.erase(0, 1);
//?   cerr << (int)result.to_run.at(SIZE(result.to_run)-1) << '\n'; //? 1
  assert(result.to_run.at(SIZE(result.to_run)-1) == ']');
  result.to_run.erase(SIZE(result.to_run)-1);
  return result;
}

:(scenario read_scenario_with_bracket_in_comment)
scenario foo [
  # ']' in comment
  1:number <- copy 0:literal
]
+run: 1:number <- copy 0:literal

:(scenario read_scenario_with_bracket_in_comment_in_nested_string)
scenario foo [
  1:address:array:character <- new [# not a comment]
]
+run: 1:address:array:character <- new [# not a comment]

//:: Run scenarios when we run 'mu test'.
//: Treat the text of the scenario as a regular series of instructions.

:(before "End Tests")
time_t mu_time; time(&mu_time);
cerr << "\nMu tests: " << ctime(&mu_time);
for (long long int i = 0; i < SIZE(Scenarios); ++i) {
//?   cerr << Passed << '\n'; //? 1
//?   cerr << i << ": " << Scenarios.at(i).name << '\n'; //? 5
  run_mu_scenario(Scenarios.at(i));
  if (Passed) cerr << ".";
}

//: Convenience: run a single named scenario.
:(before "Loading Commandline Files")
if (argc == 2 && Run_tests) {
  for (long long int i = 0; i < SIZE(Scenarios); ++i) {
    if (Scenarios.at(i).name == argv[1]) {
      run_mu_scenario(Scenarios.at(i));
      return 0;
    }
  }
}

:(before "End Globals")
const scenario* Current_scenario = NULL;
:(code)
void run_mu_scenario(const scenario& s) {
  Current_scenario = &s;
  bool not_already_inside_test = !Trace_stream;
//?   cerr << s.name << '\n'; //? 6
  if (not_already_inside_test) {
    Trace_file = s.name;
    Trace_stream = new trace_stream;
    setup();
  }
  assert(Routines.empty());
  vector<recipe_ordinal> tmp = load("recipe "+s.name+" [ "+s.to_run+" ]");
  bind_special_scenario_names(tmp.at(0));
  transform_all();
  run(tmp.front());
  if (not_already_inside_test && Trace_stream) {
    teardown();
    ofstream fout((Trace_dir+Trace_file).c_str());
    fout << Trace_stream->readable_contents("");
    fout.close();
    delete Trace_stream;
    Trace_stream = NULL;
    Trace_file = "";
  }
  Current_scenario = NULL;
}

//:: The special instructions we want to support inside scenarios.
//: In a compiler for the mu VM these will require more work.

//: 'run' interprets a string as a set of instructions

:(scenarios run)
:(scenario run)
#? % Trace_stream->dump_layer = "all";
recipe main [
  run [
    1:number <- copy 13:literal
  ]
]
+mem: storing 13 in location 1

:(before "End Primitive Recipe Declarations")
RUN,
:(before "End Primitive Recipe Numbers")
Recipe_ordinal["run"] = RUN;
:(before "End Primitive Recipe Implementations")
case RUN: {
//?   cout << "recipe " << current_instruction().ingredients.at(0).name << '\n'; //? 1
  ostringstream tmp;
  tmp << "recipe run" << Next_recipe_ordinal << " [ " << current_instruction().ingredients.at(0).name << " ]";
//?   Show_rest_of_stream = true; //? 1
  vector<recipe_ordinal> tmp_recipe = load(tmp.str());
  bind_special_scenario_names(tmp_recipe.at(0));
  transform_all();
//?   cout << tmp_recipe.at(0) << ' ' << Recipe_ordinal["main"] << '\n'; //? 1
  Current_routine->calls.push_front(call(tmp_recipe.at(0)));
  continue;  // not done with caller; don't increment current_step_index()
}

// Some variables for fake resources always get special addresses in
// scenarios.
:(code)
void bind_special_scenario_names(recipe_ordinal r) {
  // Special Scenario Variable Names(r)
  // End Special Scenario Variable Names(r)
}

:(scenario run_multiple)
recipe main [
  run [
    1:number <- copy 13:literal
  ]
  run [
    2:number <- copy 13:literal
  ]
]
+mem: storing 13 in location 1
+mem: storing 13 in location 2

//: 'memory-should-contain' raises warnings if specific locations aren't as expected
//: Also includes some special support for checking strings.

:(before "End Globals")
bool Scenario_testing_scenario = false;
:(before "End Setup")
Scenario_testing_scenario = false;

:(scenario memory_check)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  memory-should-contain [
    1 <- 13
  ]
]
+run: checking location 1
+warn: expected location 1 to contain 13 but saw 0

:(before "End Primitive Recipe Declarations")
MEMORY_SHOULD_CONTAIN,
:(before "End Primitive Recipe Numbers")
Recipe_ordinal["memory-should-contain"] = MEMORY_SHOULD_CONTAIN;
:(before "End Primitive Recipe Implementations")
case MEMORY_SHOULD_CONTAIN: {
  if (!Passed) break;
//?   cout << current_instruction().ingredients.at(0).name << '\n'; //? 1
  check_memory(current_instruction().ingredients.at(0).name);
  break;
}

:(code)
void check_memory(const string& s) {
  istringstream in(s);
  in >> std::noskipws;
  set<long long int> locations_checked;
  while (true) {
    skip_whitespace_and_comments(in);
    if (in.eof()) break;
    string lhs = next_word(in);
    if (!is_integer(lhs)) {
      check_type(lhs, in);
      continue;
    }
    int address = to_integer(lhs);
    skip_whitespace_and_comments(in);
    string _assign;  in >> _assign;  assert(_assign == "<-");
    skip_whitespace_and_comments(in);
    int value = 0;  in >> value;
    if (locations_checked.find(address) != locations_checked.end())
      raise << "duplicate expectation for location " << address << '\n';
    trace(Primitive_recipe_depth, "run") << "checking location " << address;
    if (Memory[address] != value) {
      if (Current_scenario && !Scenario_testing_scenario) {
        // genuine test in a mu file
        raise << "\nF - " << Current_scenario->name << ": expected location " << address << " to contain " << value << " but saw " << Memory[address] << '\n';
      }
      else {
        // just testing scenario support
        raise << "expected location " << address << " to contain " << value << " but saw " << Memory[address] << '\n';
      }
      if (!Scenario_testing_scenario) {
        Passed = false;
        ++Num_failures;
      }
      return;
    }
    locations_checked.insert(address);
  }
}

void check_type(const string& lhs, istream& in) {
  reagent x(lhs);
  if (x.properties.at(0).second.at(0) == "string") {
    x.set_value(to_integer(x.name));
    skip_whitespace_and_comments(in);
    string _assign = next_word(in);
    assert(_assign == "<-");
    skip_whitespace_and_comments(in);
    string literal = next_word(in);
    long long int address = x.value;
    // exclude quoting brackets
    assert(*literal.begin() == '[');  literal.erase(literal.begin());
    assert(*--literal.end() == ']');  literal.erase(--literal.end());
    check_string(address, literal);
    return;
  }
  raise << "don't know how to check memory for " << lhs << '\n';
}

void check_string(long long int address, const string& literal) {
  trace(Primitive_recipe_depth, "run") << "checking string length at " << address;
  if (Memory[address] != SIZE(literal)) {
    if (Current_scenario && !Scenario_testing_scenario)
      raise << "\nF - " << Current_scenario->name << ": expected location " << address << " to contain length " << SIZE(literal) << " of string [" << literal << "] but saw " << Memory[address] << '\n';
    else
      raise << "expected location " << address << " to contain length " << SIZE(literal) << " of string [" << literal << "] but saw " << Memory[address] << '\n';
    if (!Scenario_testing_scenario) {
      Passed = false;
      ++Num_failures;
    }
    return;
  }
  ++address;  // now skip length
  for (long long int i = 0; i < SIZE(literal); ++i) {
    trace(Primitive_recipe_depth, "run") << "checking location " << address+i;
    if (Memory[address+i] != literal.at(i)) {
      if (Current_scenario && !Scenario_testing_scenario) {
        // genuine test in a mu file
        raise << "\nF - " << Current_scenario->name << ": expected location " << (address+i) << " to contain " << literal.at(i) << " but saw " << Memory[address+i] << '\n';
      }
      else {
        // just testing scenario support
        raise << "expected location " << (address+i) << " to contain " << literal.at(i) << " but saw " << Memory[address+i] << '\n';
      }
      if (!Scenario_testing_scenario) {
        Passed = false;
        ++Num_failures;
      }
      return;
    }
  }
}

:(scenario memory_check_multiple)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  memory-should-contain [
    1 <- 0
    1 <- 0
  ]
]
+warn: duplicate expectation for location 1

:(scenario memory_check_string_length)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  1:number <- copy 3:literal
  2:number <- copy 97:literal  # 'a'
  3:number <- copy 98:literal  # 'b'
  4:number <- copy 99:literal  # 'c'
  memory-should-contain [
    1:string <- [ab]
  ]
]
+warn: expected location 1 to contain length 2 of string [ab] but saw 3

:(scenario memory_check_string)
recipe main [
  1:number <- copy 3:literal
  2:number <- copy 97:literal  # 'a'
  3:number <- copy 98:literal  # 'b'
  4:number <- copy 99:literal  # 'c'
  memory-should-contain [
    1:string <- [abc]
  ]
]
+run: checking string length at 1
+run: checking location 2
+run: checking location 3
+run: checking location 4

:(code)
//: 'trace-should-contain' is like the '+' lines in our scenarios so far
// Like runs of contiguous '+' lines, order is important. The trace checks
// that the lines are present *and* in the specified sequence. (There can be
// other lines in between.)

:(scenario trace_check_warns_on_failure)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  trace-should-contain [
    a: b
    a: d
  ]
]
+warn: missing [b] in trace layer a

:(before "End Primitive Recipe Declarations")
TRACE_SHOULD_CONTAIN,
:(before "End Primitive Recipe Numbers")
Recipe_ordinal["trace-should-contain"] = TRACE_SHOULD_CONTAIN;
:(before "End Primitive Recipe Implementations")
case TRACE_SHOULD_CONTAIN: {
  if (!Passed) break;
  check_trace(current_instruction().ingredients.at(0).name);
  break;
}

:(code)
// simplified version of check_trace_contents() that emits warnings rather
// than just printing to stderr
bool check_trace(const string& expected) {
//?   cerr << "AAA " << expected << '\n'; //? 1
  Trace_stream->newline();
  vector<trace_line> expected_lines = parse_trace(expected);
//?   cerr << "BBB " << SIZE(expected_lines) << '\n'; //? 1
  if (expected_lines.empty()) return true;
  long long int curr_expected_line = 0;
  for (vector<trace_line>::iterator p = Trace_stream->past_lines.begin(); p != Trace_stream->past_lines.end(); ++p) {
    if (expected_lines.at(curr_expected_line).label != p->label) continue;
    if (expected_lines.at(curr_expected_line).contents != trim(p->contents)) continue;
    // match
    ++curr_expected_line;
    if (curr_expected_line == SIZE(expected_lines)) {
//?       cerr << "ZZZ\n"; //? 1
      return true;
    }
  }

  raise << "missing [" << expected_lines.at(curr_expected_line).contents << "] "
        << "in trace layer " << expected_lines.at(curr_expected_line).label << '\n';
  Passed = false;
  return false;
}

vector<trace_line> parse_trace(const string& expected) {
  vector<string> buf = split(expected, "\n");
  vector<trace_line> result;
  for (long long int i = 0; i < SIZE(buf); ++i) {
    buf.at(i) = trim(buf.at(i));
    if (buf.at(i).empty()) continue;
    long long int delim = buf.at(i).find(": ");
    result.push_back(trace_line(trim(buf.at(i).substr(0, delim)),  trim(buf.at(i).substr(delim+2))));
  }
  return result;
}

:(scenario trace_check_warns_on_failure_in_later_line)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  run [
    trace [a], [b]
  ]
  trace-should-contain [
    a: b
    a: d
  ]
]
+warn: missing [d] in trace layer a

:(scenario trace_check_passes_silently)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  run [
    trace [a], [b]
  ]
  trace-should-contain [
    a: b
  ]
]
-warn: missing [b] in trace layer a
$warn: 0

//: 'trace-should-not-contain' is like the '-' lines in our scenarios so far
//: Each trace line is separately checked for absense. Order is *not*
//: important, so you can't say things like "B should not exist after A."

:(scenario trace_negative_check_warns_on_failure)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  run [
    trace [a], [b]
  ]
  trace-should-not-contain [
    a: b
  ]
]
+warn: unexpected [b] in trace layer a

:(before "End Primitive Recipe Declarations")
TRACE_SHOULD_NOT_CONTAIN,
:(before "End Primitive Recipe Numbers")
Recipe_ordinal["trace-should-not-contain"] = TRACE_SHOULD_NOT_CONTAIN;
:(before "End Primitive Recipe Implementations")
case TRACE_SHOULD_NOT_CONTAIN: {
  if (!Passed) break;
  check_trace_missing(current_instruction().ingredients.at(0).name);
  break;
}

:(code)
// simplified version of check_trace_contents() that emits warnings rather
// than just printing to stderr
bool check_trace_missing(const string& in) {
  Trace_stream->newline();
  vector<trace_line> lines = parse_trace(in);
  for (long long int i = 0; i < SIZE(lines); ++i) {
    if (trace_count(lines.at(i).label, lines.at(i).contents) != 0) {
      raise << "unexpected [" << lines.at(i).contents << "] in trace layer " << lines.at(i).label << '\n';
      Passed = false;
      return false;
    }
  }
  return true;
}

:(scenario trace_negative_check_passes_silently)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  trace-should-not-contain [
    a: b
  ]
]
-warn: unexpected [b] in trace layer a
$warn: 0

:(scenario trace_negative_check_warns_on_any_unexpected_line)
% Scenario_testing_scenario = true;
% Hide_warnings = true;
recipe main [
  run [
    trace [a], [d]
  ]
  trace-should-not-contain [
    a: b
    a: d
  ]
]
+warn: unexpected [d] in trace layer a

//:: Helpers

:(code)
// just for the scenarios running scenarios in C++ layers
void run_mu_scenario(const string& form) {
//?   cerr << form << '\n'; //? 1
  istringstream in(form);
  in >> std::noskipws;
  skip_whitespace_and_comments(in);
  string _scenario = next_word(in);
//?   cout << _scenario << '\n'; //? 2
  assert(_scenario == "scenario");
  scenario s = parse_scenario(in);
  run_mu_scenario(s);
}
