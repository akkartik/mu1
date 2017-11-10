//: Take raw control of the text-mode display and console, putting it in
//: 'console' mode rather than the usual automatically-scrolling 'typewriter'
//: mode.

//:: Display management

:(before "End Globals")
int Display_row = 0;
int Display_column = 0;

:(before "End Includes")
#define CHECK_SCREEN \
    if (!tb_is_active()) { \
      if (Run_tests) \
        raise << maybe(current_recipe_name()) << "tried to print to real screen in a test!\n" << end(); \
      else \
        raise << maybe(current_recipe_name()) << "tried to print to real screen before 'open-console' or after 'close-console'\n" << end(); \
      break; \
    }
#define CHECK_CONSOLE \
    if (!tb_is_active()) { \
      if (Run_tests) \
        raise << maybe(current_recipe_name()) << "tried to read event from real keyboard/mouse in a test!\n" << end(); \
      else \
        raise << maybe(current_recipe_name()) << "tried to read event from real keyboard/mouse before 'open-console' or after 'close-console'\n" << end(); \
      break; \
    }

:(before "End Primitive Recipe Declarations")
OPEN_CONSOLE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "open-console", OPEN_CONSOLE);
:(before "End Primitive Recipe Checks")
case OPEN_CONSOLE: {
  break;
}
:(before "End Primitive Recipe Implementations")
case OPEN_CONSOLE: {
  tb_init();
  std::setvbuf(stdout, NULL, _IONBF, 0);  // disable buffering in cout
  Display_row = Display_column = 0;
  int width = tb_width();
  int height = tb_height();
  if (width > 222 || height > 222) {
    if (width > 222)
      raise << "sorry, Mu doesn't support windows wider than 222 characters in console mode. Please resize your window.\n" << end();
    if (height > 222)
      raise << "sorry, Mu doesn't support windows taller than 222 characters in console mode. Please resize your window.\n" << end();
    exit(1);
  }
  break;
}

:(before "End Primitive Recipe Declarations")
CLOSE_CONSOLE,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "close-console", CLOSE_CONSOLE);
:(before "End Primitive Recipe Checks")
case CLOSE_CONSOLE: {
  break;
}
:(before "End Primitive Recipe Implementations")
case CLOSE_CONSOLE: {
  tb_shutdown();
  break;
}

:(before "End Primitive Recipe Declarations")
CLEAR_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "clear-display", CLEAR_DISPLAY);
:(before "End Primitive Recipe Checks")
case CLEAR_DISPLAY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case CLEAR_DISPLAY: {
  CHECK_SCREEN;
  tb_clear();
  Display_row = Display_column = 0;
  break;
}

:(before "End Primitive Recipe Declarations")
PRINT_CHARACTER_TO_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "print-character-to-display", PRINT_CHARACTER_TO_DISPLAY);
:(before "End Primitive Recipe Checks")
case PRINT_CHARACTER_TO_DISPLAY: {
  if (inst.ingredients.empty()) {
    raise << maybe(get(Recipe, r).name) << "'print-character-to-display' requires at least one ingredient, but got '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!is_mu_number(inst.ingredients.at(0))) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'print-character-to-display' should be a character, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  if (SIZE(inst.ingredients) > 1) {
    if (!is_mu_number(inst.ingredients.at(1))) {
      raise << maybe(get(Recipe, r).name) << "second ingredient of 'print-character-to-display' should be a foreground color number, but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
      break;
    }
  }
  if (SIZE(inst.ingredients) > 2) {
    if (!is_mu_number(inst.ingredients.at(2))) {
      raise << maybe(get(Recipe, r).name) << "third ingredient of 'print-character-to-display' should be a background color number, but got '" << inst.ingredients.at(2).original_string << "'\n" << end();
      break;
    }
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case PRINT_CHARACTER_TO_DISPLAY: {
  CHECK_SCREEN;
  int h=tb_height(), w=tb_width();
  int height = (h >= 0) ? h : 0;
  int width = (w >= 0) ? w : 0;
  int c = ingredients.at(0).at(0);
  int color = TB_WHITE;
  if (SIZE(ingredients) > 1) {
    color = ingredients.at(1).at(0);
  }
  int bg_color = TB_BLACK;
  if (SIZE(ingredients) > 2) {
    bg_color = ingredients.at(2).at(0);
    if (bg_color == 0) bg_color = TB_BLACK;
  }
  tb_print(c, color, bg_color);
  // track row and column, mimicking what happens on screen
  if (c == '\n') {
    if (Display_row < height-1) ++Display_row;  // otherwise we scroll and Display_row remains unchanged
  }
  else if (c == '\r') {
    Display_column = 0;
  }
  else if (c == '\b') {
    if (Display_column > 0) --Display_column;
  }
  else {
    ++Display_column;
    if (Display_column >= width) {
      Display_column = 0;
      if (Display_row < height-1) ++Display_row;
    }
  }
  break;
}

:(before "End Primitive Recipe Declarations")
CURSOR_POSITION_ON_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "cursor-position-on-display", CURSOR_POSITION_ON_DISPLAY);
:(before "End Primitive Recipe Checks")
case CURSOR_POSITION_ON_DISPLAY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case CURSOR_POSITION_ON_DISPLAY: {
  CHECK_SCREEN;
  products.resize(2);
  products.at(0).push_back(Display_row);
  products.at(1).push_back(Display_column);
  break;
}

:(before "End Primitive Recipe Declarations")
MOVE_CURSOR_ON_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "move-cursor-on-display", MOVE_CURSOR_ON_DISPLAY);
:(before "End Primitive Recipe Checks")
case MOVE_CURSOR_ON_DISPLAY: {
  if (SIZE(inst.ingredients) != 2) {
    raise << maybe(get(Recipe, r).name) << "'move-cursor-on-display' requires two ingredients, but got '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!is_mu_number(inst.ingredients.at(0))) {
    raise << maybe(get(Recipe, r).name) << "first ingredient of 'move-cursor-on-display' should be a row number, but got '" << inst.ingredients.at(0).original_string << "'\n" << end();
    break;
  }
  if (!is_mu_number(inst.ingredients.at(1))) {
    raise << maybe(get(Recipe, r).name) << "second ingredient of 'move-cursor-on-display' should be a column number, but got '" << inst.ingredients.at(1).original_string << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case MOVE_CURSOR_ON_DISPLAY: {
  CHECK_SCREEN;
  Display_row = ingredients.at(0).at(0);
  Display_column = ingredients.at(1).at(0);
  tb_set_cursor(Display_column, Display_row);
  break;
}

:(before "End Primitive Recipe Declarations")
MOVE_CURSOR_DOWN_ON_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "move-cursor-down-on-display", MOVE_CURSOR_DOWN_ON_DISPLAY);
:(before "End Primitive Recipe Checks")
case MOVE_CURSOR_DOWN_ON_DISPLAY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case MOVE_CURSOR_DOWN_ON_DISPLAY: {
  CHECK_SCREEN;
  int h=tb_height();
  int height = (h >= 0) ? h : 0;
  if (Display_row < height-1) {
    ++Display_row;
    tb_set_cursor(Display_column, Display_row);
  }
  break;
}

:(before "End Primitive Recipe Declarations")
MOVE_CURSOR_UP_ON_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "move-cursor-up-on-display", MOVE_CURSOR_UP_ON_DISPLAY);
:(before "End Primitive Recipe Checks")
case MOVE_CURSOR_UP_ON_DISPLAY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case MOVE_CURSOR_UP_ON_DISPLAY: {
  CHECK_SCREEN;
  if (Display_row > 0) {
    --Display_row;
    tb_set_cursor(Display_column, Display_row);
  }
  break;
}

:(before "End Primitive Recipe Declarations")
MOVE_CURSOR_RIGHT_ON_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "move-cursor-right-on-display", MOVE_CURSOR_RIGHT_ON_DISPLAY);
:(before "End Primitive Recipe Checks")
case MOVE_CURSOR_RIGHT_ON_DISPLAY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case MOVE_CURSOR_RIGHT_ON_DISPLAY: {
  CHECK_SCREEN;
  int w=tb_width();
  int width = (w >= 0) ? w : 0;
  if (Display_column < width-1) {
    ++Display_column;
    tb_set_cursor(Display_column, Display_row);
  }
  break;
}

:(before "End Primitive Recipe Declarations")
MOVE_CURSOR_LEFT_ON_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "move-cursor-left-on-display", MOVE_CURSOR_LEFT_ON_DISPLAY);
:(before "End Primitive Recipe Checks")
case MOVE_CURSOR_LEFT_ON_DISPLAY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case MOVE_CURSOR_LEFT_ON_DISPLAY: {
  CHECK_SCREEN;
  if (Display_column > 0) {
    --Display_column;
    tb_set_cursor(Display_column, Display_row);
  }
  break;
}

//: as a convenience, make $print mostly work in console mode
:(before "End $print 10/newline Special-cases")
else if (tb_is_active()) {
  move_cursor_to_start_of_next_line_on_display();
}
:(code)
void move_cursor_to_start_of_next_line_on_display() {
  if (Display_row < tb_height()-1) ++Display_row;
  else Display_row = 0;
  Display_column = 0;
  tb_set_cursor(Display_column, Display_row);
}

:(before "End Primitive Recipe Declarations")
DISPLAY_WIDTH,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "display-width", DISPLAY_WIDTH);
:(before "End Primitive Recipe Checks")
case DISPLAY_WIDTH: {
  break;
}
:(before "End Primitive Recipe Implementations")
case DISPLAY_WIDTH: {
  CHECK_SCREEN;
  products.resize(1);
  products.at(0).push_back(tb_width());
  break;
}

:(before "End Primitive Recipe Declarations")
DISPLAY_HEIGHT,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "display-height", DISPLAY_HEIGHT);
:(before "End Primitive Recipe Checks")
case DISPLAY_HEIGHT: {
  break;
}
:(before "End Primitive Recipe Implementations")
case DISPLAY_HEIGHT: {
  CHECK_SCREEN;
  products.resize(1);
  products.at(0).push_back(tb_height());
  break;
}

//:: Keyboard/mouse management

:(before "End Primitive Recipe Declarations")
WAIT_FOR_SOME_INTERACTION,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "wait-for-some-interaction", WAIT_FOR_SOME_INTERACTION);
:(before "End Primitive Recipe Checks")
case WAIT_FOR_SOME_INTERACTION: {
  break;
}
:(before "End Primitive Recipe Implementations")
case WAIT_FOR_SOME_INTERACTION: {
  CHECK_SCREEN;
  tb_event event;
  tb_poll_event(&event);
  break;
}

:(before "End Primitive Recipe Declarations")
CHECK_FOR_INTERACTION,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "check-for-interaction", CHECK_FOR_INTERACTION);
:(before "End Primitive Recipe Checks")
case CHECK_FOR_INTERACTION: {
  break;
}
:(before "End Primitive Recipe Implementations")
case CHECK_FOR_INTERACTION: {
  CHECK_CONSOLE;
  products.resize(2);  // result and status
  tb_event event;
  int event_type = tb_peek_event(&event, 5/*ms*/);
  if (event_type == TB_EVENT_KEY && event.ch) {
    products.at(0).push_back(/*text event*/0);
    products.at(0).push_back(event.ch);
    products.at(0).push_back(0);
    products.at(0).push_back(0);
    products.at(1).push_back(/*found*/true);
    break;
  }
  // treat keys within ascii as unicode characters
  if (event_type == TB_EVENT_KEY && event.key < 0xff) {
    products.at(0).push_back(/*text event*/0);
    if (event.key == TB_KEY_CTRL_C) exit(1);
    if (event.key == TB_KEY_BACKSPACE2) event.key = TB_KEY_BACKSPACE;
    if (event.key == TB_KEY_CARRIAGE_RETURN) event.key = TB_KEY_NEWLINE;
    products.at(0).push_back(event.key);
    products.at(0).push_back(0);
    products.at(0).push_back(0);
    products.at(1).push_back(/*found*/true);
    break;
  }
  // keys outside ascii aren't unicode characters but arbitrary termbox inventions
  if (event_type == TB_EVENT_KEY) {
    products.at(0).push_back(/*keycode event*/1);
    products.at(0).push_back(event.key);
    products.at(0).push_back(0);
    products.at(0).push_back(0);
    products.at(1).push_back(/*found*/true);
    break;
  }
  if (event_type == TB_EVENT_MOUSE) {
    products.at(0).push_back(/*touch event*/2);
    products.at(0).push_back(event.key);  // which button, etc.
    products.at(0).push_back(event.y);  // row
    products.at(0).push_back(event.x);  // column
    products.at(1).push_back(/*found*/true);
    break;
  }
  if (event_type == TB_EVENT_RESIZE) {
    products.at(0).push_back(/*resize event*/3);
    products.at(0).push_back(event.w);  // width
    products.at(0).push_back(event.h);  // height
    products.at(0).push_back(0);
    products.at(1).push_back(/*found*/true);
    break;
  }
  assert(event_type == 0);
  products.at(0).push_back(0);
  products.at(0).push_back(0);
  products.at(0).push_back(0);
  products.at(0).push_back(0);
  products.at(1).push_back(/*found*/false);
  break;
}

:(before "End Primitive Recipe Declarations")
INTERACTIONS_LEFT,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "interactions-left?", INTERACTIONS_LEFT);
:(before "End Primitive Recipe Checks")
case INTERACTIONS_LEFT: {
  break;
}
:(before "End Primitive Recipe Implementations")
case INTERACTIONS_LEFT: {
  CHECK_CONSOLE;
  products.resize(1);
  products.at(0).push_back(tb_event_ready());
  break;
}

//: hacks to make text-mode apps more responsive under Unix

:(before "End Primitive Recipe Declarations")
CLEAR_LINE_ON_DISPLAY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "clear-line-on-display", CLEAR_LINE_ON_DISPLAY);
:(before "End Primitive Recipe Checks")
case CLEAR_LINE_ON_DISPLAY: {
  break;
}
:(before "End Primitive Recipe Implementations")
case CLEAR_LINE_ON_DISPLAY: {
  CHECK_SCREEN;
  int width = tb_width();
  for (int x = Display_column;  x < width;  ++x)
    tb_print(' ', TB_WHITE, TB_BLACK);
  tb_set_cursor(Display_column, Display_row);
  break;
}

:(before "End Primitive Recipe Declarations")
CLEAR_DISPLAY_FROM,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "clear-display-from", CLEAR_DISPLAY_FROM);
:(before "End Primitive Recipe Checks")
case CLEAR_DISPLAY_FROM: {
  break;
}
:(before "End Primitive Recipe Implementations")
case CLEAR_DISPLAY_FROM: {
  CHECK_SCREEN;
  // todo: error checking
  int row = ingredients.at(0).at(0);
  int column = ingredients.at(1).at(0);
  int left = ingredients.at(2).at(0);
  int right = ingredients.at(3).at(0);
  int height=tb_height();
  for (/*nada*/;  row < height;  ++row, column=left) {  // start column from left in every inner loop except first
    tb_set_cursor(column, row);
    for (/*nada*/;  column <= right;  ++column)
      tb_print(' ', TB_WHITE, TB_BLACK);
  }
  tb_set_cursor(Display_column, Display_row);
  break;
}
