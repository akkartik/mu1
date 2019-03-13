//: Clean syntax to manipulate and check the screen in scenarios.
//: Instructions 'assume-screen' and 'screen-should-contain' implicitly create
//: a variable called 'screen' that is accessible to later instructions in the
//: scenario. 'screen-should-contain' can check unicode characters in the fake
//: screen

//: first make sure we don't mangle these instructions in other transforms
:(before "End initialize_transform_rewrite_literal_string_to_text()")
recipes_taking_literal_strings.insert("screen-should-contain");
recipes_taking_literal_strings.insert("screen-should-contain-in-color");

:(code)
void test_screen_in_scenario() {
  run_mu_scenario(
      "scenario screen-in-scenario [\n"
      "  local-scope\n"
      "  assume-screen 5/width, 3/height\n"
      "  run [\n"
      "    a:char <- copy 97/a\n"
      "    screen:&:screen <- print screen:&:screen, a\n"
      "  ]\n"
      "  screen-should-contain [\n"
      //    01234
      "    .a    .\n"
      "    .     .\n"
      "    .     .\n"
      "  ]\n"
      "]\n"
  );
  // checks are inside scenario
}

void test_screen_in_scenario_unicode() {
  // screen-should-contain can check unicode characters in the fake screen\n"
  run_mu_scenario(
      "scenario screen-in-scenario-unicode [\n"
      "  local-scope\n"
      "  assume-screen 5/width, 3/height\n"
      "  run [\n"
      "    lambda:char <- copy 955/greek-small-lambda\n"
      "    screen:&:screen <- print screen:&:screen, lambda\n"
      "    a:char <- copy 97/a\n"
      "    screen:&:screen <- print screen:&:screen, a\n"
      "  ]\n"
      "  screen-should-contain [\n"
      //    01234
      "    .λa   .\n"
      "    .     .\n"
      "    .     .\n"
      "  ]\n"
      "]\n"
  );
  // checks are inside scenario
}

void test_screen_in_scenario_color() {
  run_mu_scenario(
      "scenario screen-in-scenario-color [\n"
      "  local-scope\n"
      "  assume-screen 5/width, 3/height\n"
      "  run [\n"
      "    lambda:char <- copy 955/greek-small-lambda\n"
      "    screen:&:screen <- print screen:&:screen, lambda, 1/red\n"
      "    a:char <- copy 97/a\n"
      "    screen:&:screen <- print screen:&:screen, a, 7/white\n"
      "  ]\n"
         // screen-should-contain shows everything
      "  screen-should-contain [\n"
      //    01234
      "    .λa   .\n"
      "    .     .\n"
      "    .     .\n"
      "  ]\n"
         // screen-should-contain-in-color filters out everything except the
         // given color, all you see is the 'a' in white.
      "  screen-should-contain-in-color 7/white, [\n"
      //    01234
      "    . a   .\n"
      "    .     .\n"
      "    .     .\n"
      "  ]\n"
         // ..and the λ in red.
      "  screen-should-contain-in-color 1/red, [\n"
      //    01234
      "    .λ    .\n"
      "    .     .\n"
      "    .     .\n"
      "  ]\n"
      "]\n"
  );
  // checks are inside scenario
}

void test_screen_in_scenario_error() {
  Scenario_testing_scenario = true;
  Hide_errors = true;
  run_mu_scenario(
      "scenario screen-in-scenario-error [\n"
      "  local-scope\n"
      "  assume-screen 5/width, 3/height\n"
      "  run [\n"
      "    a:char <- copy 97/a\n"
      "    screen:&:screen <- print screen:&:screen, a\n"
      "  ]\n"
      "  screen-should-contain [\n"
      //    01234
      "    .b    .\n"
      "    .     .\n"
      "    .     .\n"
      "  ]\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: F - screen-in-scenario-error: expected screen location (0, 0) to contain 98 ('b') instead of 97 ('a')\n"
  );
}

void test_screen_in_scenario_color_error() {
  Scenario_testing_scenario = true;
  Hide_errors = true;
  run_mu_scenario(
      "scenario screen-in-scenario-color-error [\n"
      "  local-scope\n"
      "  assume-screen 5/width, 3/height\n"
      "  run [\n"
      "    a:char <- copy 97/a\n"
      "    screen:&:screen <- print screen:&:screen, a, 1/red\n"
      "  ]\n"
      "  screen-should-contain-in-color 2/green, [\n"
      //    01234
      "    .a    .\n"
      "    .     .\n"
      "    .     .\n"
      "  ]\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: F - screen-in-scenario-color-error: expected screen location (0, 0) to contain 'a' in color 2 instead of 1\n"
  );
}

void test_convert_names_does_not_fail_when_mixing_special_names_and_numeric_locations() {
  Scenario_testing_scenario = true;
  run(
      "def main [\n"
      "  screen:num <- copy 1:num\n"
      "]\n"
  );
  CHECK_TRACE_DOESNT_CONTAIN("error: mixing variable names and numeric addresses in main");
  CHECK_TRACE_COUNT("error", 0);
}

//: It's easier to implement assume-screen and other similar scenario-only
//: primitives if they always write to a fixed location. So we'll assign a
//: single fixed location for the per-scenario screen, keyboard, file system,
//: etc. Carve space for these fixed locations out of the reserved-for-test
//: locations.

:(before "End Globals")
extern const int Max_variables_in_scenarios = Reserved_for_tests-100;
int Next_predefined_global_for_scenarios = Max_variables_in_scenarios;
:(before "End Reset")
assert(Next_predefined_global_for_scenarios < Reserved_for_tests);

:(before "End Globals")
// Scenario Globals.
extern const int SCREEN = next_predefined_global_for_scenarios(/*size_of(address:screen)*/2);
// End Scenario Globals.
:(code)
int next_predefined_global_for_scenarios(int size) {
  int result = Next_predefined_global_for_scenarios;
  Next_predefined_global_for_scenarios += size;
  return result;
}

//: give 'screen' a fixed location in scenarios
:(before "End Special Scenario Variable Names(r)")
Name[r]["screen"] = SCREEN;
//: make 'screen' always a raw location in scenarios
:(before "End is_special_name Special-cases")
if (s == "screen") return true;

:(before "End Rewrite Instruction(curr, recipe result)")
// rewrite 'assume-screen width, height' to
// 'screen:&:screen <- new-fake-screen width, height'
if (curr.name == "assume-screen") {
  curr.name = "new-fake-screen";
  if (!curr.products.empty()) {
    raise << result.name << ": 'assume-screen' has no products\n" << end();
  }
  else if (!starts_with(result.name, "scenario_")) {
    raise << result.name << ": 'assume-screen' can't be called here, only in scenarios\n" << end();
  }
  else {
    assert(curr.products.empty());
    curr.products.push_back(reagent("screen:&:screen/raw"));
    curr.products.at(0).set_value(SCREEN);
  }
}

:(code)
void test_assume_screen_shows_up_in_errors() {
  Hide_errors = true;
  run_mu_scenario(
      "scenario assume-screen-shows-up-in-errors [\n"
      "  assume-screen width, 5\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "error: assume-screen-shows-up-in-errors: missing type for 'width' in 'assume-screen width, 5'\n"
  );
}

//: screen-should-contain is a regular instruction
:(before "End Primitive Recipe Declarations")
SCREEN_SHOULD_CONTAIN,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "screen-should-contain", SCREEN_SHOULD_CONTAIN);
:(before "End Primitive Recipe Checks")
case SCREEN_SHOULD_CONTAIN: {
  if (SIZE(inst.ingredients) != 1) {
    raise << maybe(get(Recipe, r).name) << "'screen-should-contain' requires exactly one ingredient, but got '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!is_literal_text(inst.ingredients.at(0))) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'screen-should-contain' should be a literal string, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case SCREEN_SHOULD_CONTAIN: {
  if (!Passed) break;
  assert(scalar(ingredients.at(0)));
  check_screen(current_instruction().ingredients.at(0).name, -1);
  break;
}

:(before "End Primitive Recipe Declarations")
SCREEN_SHOULD_CONTAIN_IN_COLOR,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "screen-should-contain-in-color", SCREEN_SHOULD_CONTAIN_IN_COLOR);
:(before "End Primitive Recipe Checks")
case SCREEN_SHOULD_CONTAIN_IN_COLOR: {
  if (SIZE(inst.ingredients) != 2) {
    raise << maybe(get(Recipe, r).name) << "'screen-should-contain-in-color' requires exactly two ingredients, but got '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!is_mu_number(inst.ingredients.at(0))) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'screen-should-contain-in-color' should be a number (color code), but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  if (!is_literal_text(inst.ingredients.at(1))) {
    raise << maybe(get(Recipe, r).name) << "second ingredient of 'screen-should-contain-in-color' should be a literal string, but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case SCREEN_SHOULD_CONTAIN_IN_COLOR: {
  if (!Passed) break;
  assert(scalar(ingredients.at(0)));
  assert(scalar(ingredients.at(1)));
  check_screen(current_instruction().ingredients.at(1).name, ingredients.at(0).at(0));
  break;
}

:(before "End Types")
// scan an array of characters in a unicode-aware, bounds-checked manner
struct raw_string_stream {
  int index;
  const int max;
  const char* buf;

  raw_string_stream(const string&);
  uint32_t get();  // unicode codepoint
  uint32_t peek();  // unicode codepoint
  bool at_end() const;
  void skip_whitespace_and_comments();
};

:(code)
void check_screen(const string& expected_contents, const int color) {
  int screen_location = get_or_insert(Memory, SCREEN+/*skip address alloc id*/1) + /*skip payload alloc id*/1;
  reagent screen("x:screen");  // just to ensure screen.type is reclaimed
  int screen_data_location = find_element_location(screen_location, "data", screen.type, "check_screen");  // type: address:array:character
  assert(screen_data_location >= 0);
//?   cerr << "screen data is at location " << screen_data_location << '\n';
  int screen_data_start = get_or_insert(Memory, screen_data_location+/*skip address alloc id*/1) + /*skip payload alloc id*/1;  // type: array:character
//?   cerr << "screen data start is at " << screen_data_start << '\n';
  int screen_width_location = find_element_location(screen_location, "num-columns", screen.type, "check_screen");
//?   cerr << "screen width is at location " << screen_width_location << '\n';
  int screen_width = get_or_insert(Memory, screen_width_location);
//?   cerr << "screen width: " << screen_width << '\n';
  int screen_height_location = find_element_location(screen_location, "num-rows", screen.type, "check_screen");
//?   cerr << "screen height is at location " << screen_height_location << '\n';
  int screen_height = get_or_insert(Memory, screen_height_location);
//?   cerr << "screen height: " << screen_height << '\n';
  int top_index_location= find_element_location(screen_location, "top-idx", screen.type, "check_screen");
//?   cerr << "top of screen is at location " << top_index_location << '\n';
  int top_index = get_or_insert(Memory, top_index_location);
//?   cerr << "top of screen is index " << top_index << '\n';
  raw_string_stream cursor(expected_contents);
  // todo: too-long expected_contents should fail
  for (int i=0, row=top_index/screen_width;  i < screen_height;  ++i, row=(row+1)%screen_height) {
    cursor.skip_whitespace_and_comments();
    if (cursor.at_end()) break;
    if (cursor.get() != '.') {
      raise << maybe(current_recipe_name()) << "each row of the expected screen should start with a '.'\n" << end();
      if (!Scenario_testing_scenario) Passed = false;
      return;
    }
    int addr = screen_data_start+/*length*/1+row*screen_width* /*size of screen-cell*/2;
    for (int column = 0;  column < screen_width;  ++column, addr+= /*size of screen-cell*/2) {
      const int cell_color_offset = 1;
      uint32_t curr = cursor.get();
      if (get_or_insert(Memory, addr) == 0 && isspace(curr)) continue;
      if (curr == ' ' && color != -1 && color != get_or_insert(Memory, addr+cell_color_offset)) {
        // filter out other colors
        continue;
      }
      if (get_or_insert(Memory, addr) != 0 && get_or_insert(Memory, addr) == curr) {
        if (color == -1 || color == get_or_insert(Memory, addr+cell_color_offset)) continue;
        // contents match but color is off
        if (!Hide_errors) cerr << '\n';
        raise << "F - " << maybe(current_recipe_name()) << "expected screen location (" << row << ", " << column << ") to contain '" << unicode_character_at(addr) << "' in color " << color << " instead of " << no_scientific(get_or_insert(Memory, addr+cell_color_offset)) << "\n" << end();
        if (!Hide_errors) dump_screen();
        if (!Scenario_testing_scenario) Passed = false;
        return;
      }

      // really a mismatch
      // can't print multi-byte unicode characters in errors just yet. not very useful for debugging anyway.
      char expected_pretty[10] = {0};
      if (curr < 256 && !iscntrl(curr)) {
        // " ('<curr>')"
        expected_pretty[0] = ' ', expected_pretty[1] = '(', expected_pretty[2] = '\'', expected_pretty[3] = static_cast<unsigned char>(curr), expected_pretty[4] = '\'', expected_pretty[5] = ')', expected_pretty[6] = '\0';
      }
      char actual_pretty[10] = {0};
      if (get_or_insert(Memory, addr) < 256 && !iscntrl(get_or_insert(Memory, addr))) {
        // " ('<curr>')"
        actual_pretty[0] = ' ', actual_pretty[1] = '(', actual_pretty[2] = '\'', actual_pretty[3] = static_cast<unsigned char>(get_or_insert(Memory, addr)), actual_pretty[4] = '\'', actual_pretty[5] = ')', actual_pretty[6] = '\0';
      }

      ostringstream color_phrase;
      if (color != -1) color_phrase << " in color " << color;
      if (!Hide_errors) cerr << '\n';
      raise << "F - " << maybe(current_recipe_name()) << "expected screen location (" << row << ", " << column << ") to contain " << curr << expected_pretty << color_phrase.str() << " instead of " << no_scientific(get_or_insert(Memory, addr)) << actual_pretty << '\n' << end();
      if (!Hide_errors) dump_screen();
      if (!Scenario_testing_scenario) Passed = false;
      return;
    }
    if (cursor.get() != '.') {
      raise << maybe(current_recipe_name()) << "row " << row << " of the expected screen is too long\n" << end();
      if (!Scenario_testing_scenario) Passed = false;
      return;
    }
  }
  cursor.skip_whitespace_and_comments();
  if (!cursor.at_end()) {
    raise << maybe(current_recipe_name()) << "expected screen has too many rows\n" << end();
    Passed = false;
  }
}

const char* unicode_character_at(int addr) {
  int unicode_code_point = static_cast<int>(get_or_insert(Memory, addr));
  return to_unicode(unicode_code_point);
}

raw_string_stream::raw_string_stream(const string& backing) :index(0), max(SIZE(backing)), buf(backing.c_str()) {}

bool raw_string_stream::at_end() const {
  if (index >= max) return true;
  if (tb_utf8_char_length(buf[index]) > max-index) {
    raise << "unicode string seems corrupted at index "<< index << " character " << static_cast<int>(buf[index]) << '\n' << end();
    return true;
  }
  return false;
}

uint32_t raw_string_stream::get() {
  assert(index < max);  // caller must check bounds before calling 'get'
  uint32_t result = 0;
  int length = tb_utf8_char_to_unicode(&result, &buf[index]);
  assert(length != TB_EOF);
  index += length;
  return result;
}

uint32_t raw_string_stream::peek() {
  assert(index < max);  // caller must check bounds before calling 'get'
  uint32_t result = 0;
  int length = tb_utf8_char_to_unicode(&result, &buf[index]);
  assert(length != TB_EOF);
  return result;
}

void raw_string_stream::skip_whitespace_and_comments() {
  while (!at_end()) {
    if (isspace(peek())) get();
    else if (peek() == '#') {
      // skip comment
      get();
      while (peek() != '\n') get();  // implicitly also handles CRLF
    }
    else break;
  }
}

:(before "End Primitive Recipe Declarations")
_DUMP_SCREEN,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "$dump-screen", _DUMP_SCREEN);
:(before "End Primitive Recipe Checks")
case _DUMP_SCREEN: {
  break;
}
:(before "End Primitive Recipe Implementations")
case _DUMP_SCREEN: {
  dump_screen();
  break;
}

:(code)
void dump_screen() {
  int screen_location = get_or_insert(Memory, SCREEN+/*skip address alloc id*/1) + /*skip payload alloc id*/1;
  reagent screen("x:screen");  // just to ensure screen.type is reclaimed
  int screen_data_location = find_element_location(screen_location, "data", screen.type, "check_screen");  // type: address:array:character
  assert(screen_data_location >= 0);
//?   cerr << "screen data is at location " << screen_data_location << '\n';
  int screen_data_start = get_or_insert(Memory, screen_data_location+/*skip address alloc id*/1) + /*skip payload alloc id*/1;  // type: array:character
//?   cerr << "screen data start is at " << screen_data_start << '\n';
  int screen_width_location = find_element_location(screen_location, "num-columns", screen.type, "check_screen");
//?   cerr << "screen width is at location " << screen_width_location << '\n';
  int screen_width = get_or_insert(Memory, screen_width_location);
//?   cerr << "screen width: " << screen_width << '\n';
  int screen_height_location = find_element_location(screen_location, "num-rows", screen.type, "check_screen");
//?   cerr << "screen height is at location " << screen_height_location << '\n';
  int screen_height = get_or_insert(Memory, screen_height_location);
//?   cerr << "screen height: " << screen_height << '\n';
  int top_index_location= find_element_location(screen_location, "top-idx", screen.type, "check_screen");
//?   cerr << "top of screen is at location " << top_index_location << '\n';
  int top_index = get_or_insert(Memory, top_index_location);
//?   cerr << "top of screen is index " << top_index << '\n';
  for (int i=0, row=top_index/screen_width;  i < screen_height;  ++i, row=(row+1)%screen_height) {
    cerr << '.';
    int curr = screen_data_start+/*length*/1+row*screen_width* /*size of screen-cell*/2;
    for (int col = 0;  col < screen_width;  ++col) {
      if (get_or_insert(Memory, curr))
        cerr << to_unicode(static_cast<uint32_t>(get_or_insert(Memory, curr)));
      else
        cerr << ' ';
      curr += /*size of screen-cell*/2;
    }
    cerr << ".\n";
  }
}
