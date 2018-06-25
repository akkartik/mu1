//: Phase 1 of running Mu code: load it from a textual representation.
//:
//: The process of running Mu code:
//:   load -> transform -> run

:(scenarios load)  // use 'load' instead of 'run' in all scenarios in this layer
:(scenario first_recipe)
def main [
  1:number <- copy 23
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}

:(code)
vector<recipe_ordinal> load(string form) {
  istringstream in(form);
  in >> std::noskipws;
  return load(in);
}

vector<recipe_ordinal> load(istream& in) {
  in >> std::noskipws;
  vector<recipe_ordinal> result;
  while (has_data(in)) {
    skip_whitespace_and_comments(in);
    if (!has_data(in)) break;
    string command = next_word(in);
    if (command.empty()) {
      assert(!has_data(in));
      break;
    }
    // Command Handlers
    if (command == "recipe" || command == "def") {
      recipe_ordinal r = slurp_recipe(in);
      if (r > 0) result.push_back(r);
    }
    else if (command == "recipe!" || command == "def!") {
      Disable_redefine_checks = true;
      recipe_ordinal r = slurp_recipe(in);
      if (r > 0) result.push_back(r);
      Disable_redefine_checks = false;
    }
    // End Command Handlers
    else {
      raise << "unknown top-level command: " << command << '\n' << end();
    }
  }
  return result;
}

// return the recipe ordinal slurped, or -1 if it failed
int slurp_recipe(istream& in) {
  recipe result;
  result.name = next_word(in);
  if (result.name.empty()) {
    assert(!has_data(in));
    raise << "file ended with 'recipe'\n" << end();
    return -1;
  }
  // End Load Recipe Name
  skip_whitespace_but_not_newline(in);
  // End Recipe Refinements
  if (result.name.empty())
    raise << "empty result.name\n" << end();
  trace(9991, "parse") << "--- defining " << result.name << end();
  if (!contains_key(Recipe_ordinal, result.name))
    put(Recipe_ordinal, result.name, Next_recipe_ordinal);
  result.ordinal = get(Recipe_ordinal, result.name);
  ++Next_recipe_ordinal;
  if (Recipe.find(get(Recipe_ordinal, result.name)) != Recipe.end()) {
    trace(9991, "parse") << "already exists" << end();
    if (should_check_for_redefine(result.name))
      raise << "redefining recipe " << result.name << "\n" << end();
    Recipe.erase(get(Recipe_ordinal, result.name));
  }
  slurp_body(in, result);
  // End Recipe Body(result)
  put(Recipe, get(Recipe_ordinal, result.name), result);
  return get(Recipe_ordinal, result.name);
}

void slurp_body(istream& in, recipe& result) {
  in >> std::noskipws;
  skip_whitespace_but_not_newline(in);
  if (in.get() != '[')
    raise << result.name << ": recipe body must begin with '['\n" << end();
  skip_whitespace_and_comments(in);  // permit trailing comment after '['
  instruction curr;
  while (next_instruction(in, &curr)) {
    curr.original_string = to_original_string(curr);
    // End Rewrite Instruction(curr, recipe result)
    trace(9992, "load") << "after rewriting: " << to_string(curr) << end();
    if (!curr.is_empty()) result.steps.push_back(curr);
  }
}

bool next_instruction(istream& in, instruction* curr) {
  curr->clear();
  skip_whitespace_and_comments(in);
  if (!has_data(in)) {
    raise << "incomplete recipe at end of file (0)\n" << end();
    return false;
  }

  vector<string> words;
  while (has_data(in) && in.peek() != '\n') {
    skip_whitespace_but_not_newline(in);
    if (!has_data(in)) {
      raise << "incomplete recipe at end of file (1)\n" << end();
      return false;
    }
    string word = next_word(in);
    if (word.empty()) {
      assert(!has_data(in));
      raise << "incomplete recipe at end of file (2)\n" << end();
      return false;
    }
    words.push_back(word);
    skip_whitespace_but_not_newline(in);
  }
  skip_whitespace_and_comments(in);
  if (SIZE(words) == 1 && words.at(0) == "]")
    return false;  // end of recipe

  if (SIZE(words) == 1 && is_label_word(words.at(0))) {
    curr->is_label = true;
    curr->label = words.at(0);
    trace(9993, "parse") << "label: " << curr->label << end();
    if (!has_data(in)) {
      raise << "incomplete recipe at end of file (3)\n" << end();
      return false;
    }
    return true;
  }

  vector<string>::iterator p = words.begin();
  if (find(words.begin(), words.end(), "<-") != words.end()) {
    for (;  *p != "<-";  ++p)
      curr->products.push_back(reagent(*p));
    ++p;  // skip <-
  }

  if (p == words.end()) {
    raise << "instruction prematurely ended with '<-'\n" << end();
    return false;
  }
  curr->name = *p;  ++p;
  // curr->operation will be set at transform time

  for (;  p != words.end();  ++p)
    curr->ingredients.push_back(reagent(*p));

  trace(9993, "parse") << "instruction: " << curr->name << end();
  trace(9993, "parse") << "  number of ingredients: " << SIZE(curr->ingredients) << end();
  for (vector<reagent>::iterator p = curr->ingredients.begin();  p != curr->ingredients.end();  ++p)
    trace(9993, "parse") << "  ingredient: " << to_string(*p) << end();
  for (vector<reagent>::iterator p = curr->products.begin();  p != curr->products.end();  ++p)
    trace(9993, "parse") << "  product: " << to_string(*p) << end();
  if (!has_data(in)) {
    raise << "9: unbalanced '[' for recipe\n" << end();
    return false;
  }
  // End next_instruction(curr)
  return true;
}

// can return empty string -- only if 'in' has no more data
string next_word(istream& in) {
  skip_whitespace_but_not_newline(in);
  // End next_word Special-cases
  ostringstream out;
  slurp_word(in, out);
  skip_whitespace_and_comments_but_not_newline(in);
  string result = out.str();
  if (result != "[" && ends_with(result, '['))
    raise << "insert a space before '[' in '" << result << "'\n" << end();
  return result;
}

bool is_label_word(const string& word) {
  if (word.empty()) return false;  // error raised elsewhere
  return !isalnum(word.at(0)) && string("$_*@&,=-[]()").find(word.at(0)) == string::npos;
}

bool ends_with(const string& s, const char c) {
  if (s.empty()) return false;
  return *s.rbegin() == c;
}

:(before "End Globals")
// word boundaries
extern const string Terminators("(){}");
:(code)
void slurp_word(istream& in, ostream& out) {
  char c;
  if (has_data(in) && Terminators.find(in.peek()) != string::npos) {
    in >> c;
    out << c;
    return;
  }
  while (in >> c) {
    if (isspace(c) || Terminators.find(c) != string::npos || Ignore.find(c) != string::npos) {
      in.putback(c);
      break;
    }
    out << c;
  }
}

void skip_whitespace_and_comments(istream& in) {
  while (true) {
    if (!has_data(in)) break;
    if (isspace(in.peek())) in.get();
    else if (Ignore.find(in.peek()) != string::npos) in.get();
    else if (in.peek() == '#') skip_comment(in);
    else break;
  }
}

// confusing; move to the next line only to skip a comment, but never otherwise
void skip_whitespace_and_comments_but_not_newline(istream& in) {
  while (true) {
    if (!has_data(in)) break;
    if (in.peek() == '\n') break;
    if (isspace(in.peek())) in.get();
    else if (Ignore.find(in.peek()) != string::npos) in.get();
    else if (in.peek() == '#') skip_comment(in);
    else break;
  }
}

void skip_comment(istream& in) {
  if (has_data(in) && in.peek() == '#') {
    in.get();
    while (has_data(in) && in.peek() != '\n') in.get();
  }
}

:(scenario recipe_instead_of_def)
recipe main [
  1:number <- copy 23
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}

:(scenario parse_comment_outside_recipe)
# this comment will be dropped by the tangler, so we need a dummy recipe to stop that
def f1 [
]
# this comment will go through to 'load'
def main [
  1:number <- copy 23
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}

:(scenario parse_comment_amongst_instruction)
def main [
  # comment
  1:number <- copy 23
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}

:(scenario parse_comment_amongst_instruction_2)
def main [
  # comment
  1:number <- copy 23
  # comment
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}

:(scenario parse_comment_amongst_instruction_3)
def main [
  1:number <- copy 23
  # comment
  2:number <- copy 23
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {2: "number"}

:(scenario parse_comment_after_instruction)
def main [
  1:number <- copy 23  # comment
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}

:(scenario parse_label)
def main [
  +foo
]
+parse: label: +foo

:(scenario parse_dollar_as_recipe_name)
def main [
  $foo
]
+parse: instruction: $foo

:(scenario parse_multiple_properties)
def main [
  1:number <- copy 23/foo:bar:baz
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal", "foo": ("bar" "baz")}
+parse:   product: {1: "number"}

:(scenario parse_multiple_products)
def main [
  1:number, 2:number <- copy 23
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   product: {1: "number"}
+parse:   product: {2: "number"}

:(scenario parse_multiple_ingredients)
def main [
  1:number, 2:number <- copy 23, 4:number
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   ingredient: {4: "number"}
+parse:   product: {1: "number"}
+parse:   product: {2: "number"}

:(scenario parse_multiple_types)
def main [
  1:number, 2:address:number <- copy 23, 4:number
]
+parse: instruction: copy
+parse:   ingredient: {23: "literal"}
+parse:   ingredient: {4: "number"}
+parse:   product: {1: "number"}
+parse:   product: {2: ("address" "number")}

:(scenario parse_properties)
def main [
  1:address:number/lookup <- copy 23
]
+parse:   product: {1: ("address" "number"), "lookup": ()}

//: this test we can't represent with a scenario
:(code)
void test_parse_comment_terminated_by_eof() {
  load("recipe main [\n"
       "  a:number <- copy 34\n"
       "]\n"
       "# abc");  // no newline after comment
  cerr << ".";  // termination = success
}

:(scenario warn_on_missing_space_before_bracket)
% Hide_errors = true;
def main[
  1:number <- copy 23
]
+error: insert a space before '[' in 'main['

//: Warn if a recipe gets redefined, because large codebases can accidentally
//: step on their own toes. But there'll be many occasions later where
//: we'll want to disable the errors.
:(before "End Globals")
bool Disable_redefine_checks = false;
:(before "End Reset")
Disable_redefine_checks = false;
:(code)
bool should_check_for_redefine(const string& recipe_name) {
  if (Disable_redefine_checks) return false;
  return true;
}

:(scenario forbid_redefining_recipes)
% Hide_errors = true;
def main [
  1:number <- copy 23
]
def main [
  1:number <- copy 24
]
+error: redefining recipe main

:(scenario permit_forcibly_redefining_recipes)
def main [
  1:number <- copy 23
]
def! main [
  1:number <- copy 24
]
-error: redefining recipe main
$error: 0

:(code)
// for debugging
void show_rest_of_stream(istream& in) {
  cerr << '^';
  char c;
  while (in >> c)
    cerr << c;
  cerr << "$\n";
  exit(0);
}
