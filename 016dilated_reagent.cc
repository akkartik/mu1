//: An alternative syntax for reagents that permits whitespace in properties,
//: grouped by brackets. We'll use this ability in the next layer, when we
//: generalize types from lists to trees of properties.

void test_dilated_reagent() {
  load(
      "def main [\n"
      "  {1: number, foo: bar} <- copy 34\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "parse:   product: {1: \"number\", \"foo\": \"bar\"}\n"
  );
}

void test_load_trailing_space_after_curly_bracket() {
  load(
      "def main [\n"
      "  # line below has a space at the end\n"
      "  { \n"
      "]\n"
      "# successfully parsed\n"
  );
}

void test_dilated_reagent_with_comment() {
  load(
      "def main [\n"
      "  {1: number, foo: bar} <- copy 34  # test comment\n"
      "]\n"
  );
  CHECK_TRACE_CONTENTS(
      "parse:   product: {1: \"number\", \"foo\": \"bar\"}\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

void test_dilated_reagent_with_comment_immediately_following() {
  load(
      "def main [\n"
      "  1:number <- copy {34: literal}  # test comment\n"
      "]\n"
  );
  CHECK_TRACE_COUNT("error", 0);
}

//: First augment next_word to group balanced brackets together.

:(before "End next_word Special-cases")
if (in.peek() == '(')
  return slurp_balanced_bracket(in);
// treat curlies mostly like parens, but don't mess up labels
if (start_of_dilated_reagent(in))
  return slurp_balanced_bracket(in);

:(code)
// A curly is considered a label if it's the last thing on a line. Dilated
// reagents should remain all on one line.
bool start_of_dilated_reagent(istream& in) {
  if (in.peek() != '{') return false;
  int pos = in.tellg();
  in.get();  // slurp '{'
  skip_whitespace_but_not_newline(in);
  char next = in.peek();
  in.seekg(pos);
  return next != '\n';
}

// Assume the first letter is an open bracket, and read everything until the
// matching close bracket.
// We balance {} () and [].
string slurp_balanced_bracket(istream& in) {
  ostringstream result;
  char c;
  list<char> open_brackets;
  while (in >> c) {
    if (c == '(') open_brackets.push_back(c);
    if (c == ')') {
      if (open_brackets.empty() || open_brackets.back() != '(') {
        raise << "unbalanced ')'\n" << end();
        continue;
      }
      assert(open_brackets.back() == '(');
      open_brackets.pop_back();
    }
    if (c == '[') open_brackets.push_back(c);
    if (c == ']') {
      if (open_brackets.empty() || open_brackets.back() != '[') {
        raise << "unbalanced ']'\n" << end();
        continue;
      }
      open_brackets.pop_back();
    }
    if (c == '{') open_brackets.push_back(c);
    if (c == '}') {
      if (open_brackets.empty() || open_brackets.back() != '{') {
        raise << "unbalanced '}'\n" << end();
        continue;
      }
      open_brackets.pop_back();
    }
    result << c;
    if (open_brackets.empty()) break;
  }
  skip_whitespace_and_comments_but_not_newline(in);
  return result.str();
}

:(after "Parsing reagent(string s)")
if (starts_with(s, "{")) {
  assert(properties.empty());
  istringstream in(s);
  in >> std::noskipws;
  in.get();  // skip '{'
  name = slurp_key(in);
  if (name.empty()) {
    raise << "invalid reagent '" << s << "' without a name\n" << end();
    return;
  }
  if (name == "}") {
    raise << "invalid empty reagent '" << s << "'\n" << end();
    return;
  }
  {
    string s = next_word(in);
    if (s.empty()) {
      assert(!has_data(in));
      raise << "incomplete dilated reagent at end of file (0)\n" << end();
      return;
    }
    string_tree* type_names = new string_tree(s);
    // End Parsing Dilated Reagent Type Property(type_names)
    type = new_type_tree(type_names);
    delete type_names;
  }
  while (has_data(in)) {
    string key = slurp_key(in);
    if (key.empty()) continue;
    if (key == "}") continue;
    string s = next_word(in);
    if (s.empty()) {
      assert(!has_data(in));
      raise << "incomplete dilated reagent at end of file (1)\n" << end();
      return;
    }
    string_tree* value = new string_tree(s);
    // End Parsing Dilated Reagent Property(value)
    properties.push_back(pair<string, string_tree*>(key, value));
  }
  return;
}

:(code)
string slurp_key(istream& in) {
  string result = next_word(in);
  if (result.empty()) {
    assert(!has_data(in));
    raise << "incomplete dilated reagent at end of file (2)\n" << end();
    return result;
  }
  while (!result.empty() && *result.rbegin() == ':')
    strip_last(result);
  while (isspace(in.peek()) || in.peek() == ':')
    in.get();
  return result;
}
