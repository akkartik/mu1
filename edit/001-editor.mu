## the basic editor data structure, and how it displays text to the screen

# temporary main for this layer: just render the given text at the given
# screen dimensions, then stop
def main text:text [
  local-scope
  load-inputs
  open-console
  clear-screen null/screen  # non-scrolling app
  e:&:editor <- new-editor text, 0/left, 5/right
  render null/screen, e
  wait-for-event null/console
  close-console
]

scenario editor-renders-text-to-screen [
  local-scope
  assume-screen 10/width, 5/height
  e:&:editor <- new-editor [abc], 0/left, 10/right
  run [
    render screen, e
  ]
  screen-should-contain [
    # top line of screen reserved for menu
    .          .
    .abc       .
    .          .
  ]
]

container editor [
  # editable text: doubly linked list of characters (head contains a special sentinel)
  data:&:duplex-list:char
  top-of-screen:&:duplex-list:char
  bottom-of-screen:&:duplex-list:char
  # location before cursor inside data
  before-cursor:&:duplex-list:char

  # raw bounds of display area on screen
  # always displays from row 1 (leaving row 0 for a menu) and at most until bottom of screen
  left:num
  right:num
  bottom:num
  # raw screen coordinates of cursor
  cursor-row:num
  cursor-column:num
]

# creates a new editor widget
#   right is exclusive
def new-editor s:text, left:num, right:num -> result:&:editor [
  local-scope
  load-inputs
  # no clipping of bounds
  right <- subtract right, 1
  result <- new editor:type
  # initialize screen-related fields
  *result <- put *result, left:offset, left
  *result <- put *result, right:offset, right
  # initialize cursor coordinates
  *result <- put *result, cursor-row:offset, 1/top
  *result <- put *result, cursor-column:offset, left
  # initialize empty contents
  init:&:duplex-list:char <- push 167/§, null
  *result <- put *result, data:offset, init
  *result <- put *result, top-of-screen:offset, init
  *result <- put *result, before-cursor:offset, init
  result <- insert-text result, s
  <editor-initialization>
]

def insert-text editor:&:editor, text:text -> editor:&:editor [
  local-scope
  load-inputs
  curr:&:duplex-list:char <- get *editor, data:offset
  insert curr, text
]

scenario editor-initializes-without-data [
  local-scope
  assume-screen 5/width, 3/height
  run [
    e:&:editor <- new-editor null/data, 2/left, 5/right
    1:editor/raw <- copy *e
  ]
  memory-should-contain [
    # 1,2 (data) <- just the § sentinel
    # 3,4 (top of screen) <- the § sentinel
    # 5 (bottom of screen) <- null since text fits on screen
    5 <- 0
    6 <- 0
    # 7,8 (before cursor) <- the § sentinel
    9 <- 2  # left
    10 <- 4  # right  (inclusive)
    11 <- 0  # bottom (not set until render)
    12 <- 1  # cursor row
    13 <- 2  # cursor column
  ]
  screen-should-contain [
    .     .
    .     .
    .     .
  ]
]

# Assumes cursor should be at coordinates (cursor-row, cursor-column) and
# updates before-cursor to match. Might also move coordinates if they're
# outside text.
def render screen:&:screen, editor:&:editor -> last-row:num, last-column:num, screen:&:screen, editor:&:editor [
  local-scope
  load-inputs
  return-unless editor, 1/top, 0/left
  left:num <- get *editor, left:offset
  screen-height:num <- screen-height screen
  right:num <- get *editor, right:offset
  # traversing editor
  curr:&:duplex-list:char <- get *editor, top-of-screen:offset
  prev:&:duplex-list:char <- copy curr  # just in case curr becomes null and we can't compute prev
  curr <- next curr
  # traversing screen
  color:num <- copy 7/white
  row:num <- copy 1/top
  column:num <- copy left
  cursor-row:num <- get *editor, cursor-row:offset
  cursor-column:num <- get *editor, cursor-column:offset
  before-cursor:&:duplex-list:char <- get *editor, before-cursor:offset
  screen <- move-cursor screen, row, column
  {
    +next-character
    break-unless curr
    off-screen?:bool <- greater-or-equal row, screen-height
    break-if off-screen?
    # update editor.before-cursor
    # Doing so at the start of each iteration ensures it stays one step behind
    # the current character.
    {
      at-cursor-row?:bool <- equal row, cursor-row
      break-unless at-cursor-row?
      at-cursor?:bool <- equal column, cursor-column
      break-unless at-cursor?
      before-cursor <- copy prev
    }
    c:char <- get *curr, value:offset
    <character-c-received>
    {
      # newline? move to left rather than 0
      newline?:bool <- equal c, 10/newline
      break-unless newline?
      # adjust cursor if necessary
      {
        at-cursor-row?:bool <- equal row, cursor-row
        break-unless at-cursor-row?
        left-of-cursor?:bool <- lesser-than column, cursor-column
        break-unless left-of-cursor?
        cursor-column <- copy column
        before-cursor <- prev curr
      }
      # clear rest of line in this window
      clear-line-until screen, right
      # skip to next line
      row <- add row, 1
      column <- copy left
      screen <- move-cursor screen, row, column
      curr <- next curr
      prev <- next prev
      loop +next-character
    }
    {
      # at right? wrap. even if there's only one more letter left; we need
      # room for clicking on the cursor after it.
      at-right?:bool <- equal column, right
      break-unless at-right?
      # print wrap icon
      wrap-icon:char <- copy 8617/loop-back-to-left
      print screen, wrap-icon, 245/grey
      column <- copy left
      row <- add row, 1
      screen <- move-cursor screen, row, column
      # don't increment curr
      loop +next-character
    }
    print screen, c, color
    curr <- next curr
    prev <- next prev
    column <- add column, 1
    loop
  }
  # save first character off-screen
  *editor <- put *editor, bottom-of-screen:offset, curr
  # is cursor to the right of the last line? move to end
  {
    at-cursor-row?:bool <- equal row, cursor-row
    cursor-outside-line?:bool <- lesser-or-equal column, cursor-column
    before-cursor-on-same-line?:bool <- and at-cursor-row?, cursor-outside-line?
    above-cursor-row?:bool <- lesser-than row, cursor-row
    before-cursor?:bool <- or before-cursor-on-same-line?, above-cursor-row?
    break-unless before-cursor?
    cursor-row <- copy row
    cursor-column <- copy column
    before-cursor <- copy prev
  }
  *editor <- put *editor, bottom:offset, row
  *editor <- put *editor, cursor-row:offset, cursor-row
  *editor <- put *editor, cursor-column:offset, cursor-column
  *editor <- put *editor, before-cursor:offset, before-cursor
  clear-line-until screen, right
  row <- add row, 1
  return row, left/column
]

def clear-screen-from screen:&:screen, row:num, column:num, left:num, right:num -> screen:&:screen [
  local-scope
  load-inputs
  # if it's the real screen, use the optimized primitive
  {
    break-if screen
    clear-display-from row, column, left, right
    return
  }
  # if not, go the slower route
  screen <- move-cursor screen, row, column
  clear-line-until screen, right
  clear-rest-of-screen screen, row, left, right
]

def clear-rest-of-screen screen:&:screen, row:num, left:num, right:num -> screen:&:screen [
  local-scope
  load-inputs
  row <- add row, 1
  # if it's the real screen, use the optimized primitive
  {
    break-if screen
    clear-display-from row, left, left, right
    return
  }
  screen <- move-cursor screen, row, left
  screen-height:num <- screen-height screen
  {
    at-bottom-of-screen?:bool <- greater-or-equal row, screen-height
    break-if at-bottom-of-screen?
    screen <- move-cursor screen, row, left
    clear-line-until screen, right
    row <- add row, 1
    loop
  }
]

scenario editor-prints-multiple-lines [
  local-scope
  assume-screen 5/width, 5/height
  s:text <- new [abc
def]
  e:&:editor <- new-editor s, 0/left, 5/right
  run [
    render screen, e
  ]
  screen-should-contain [
    .     .
    .abc  .
    .def  .
    .     .
  ]
]

scenario editor-handles-offsets [
  local-scope
  assume-screen 5/width, 5/height
  e:&:editor <- new-editor [abc], 1/left, 5/right
  run [
    render screen, e
  ]
  screen-should-contain [
    .     .
    . abc .
    .     .
  ]
]

scenario editor-prints-multiple-lines-at-offset [
  local-scope
  assume-screen 5/width, 5/height
  s:text <- new [abc
def]
  e:&:editor <- new-editor s, 1/left, 5/right
  run [
    render screen, e
  ]
  screen-should-contain [
    .     .
    . abc .
    . def .
    .     .
  ]
]

scenario editor-wraps-long-lines [
  local-scope
  assume-screen 5/width, 5/height
  e:&:editor <- new-editor [abc def], 0/left, 5/right
  run [
    render screen, e
  ]
  screen-should-contain [
    .     .
    .abc ↩.
    .def  .
    .     .
  ]
  screen-should-contain-in-color 245/grey [
    .     .
    .    ↩.
    .     .
    .     .
  ]
]

scenario editor-wraps-barely-long-lines [
  local-scope
  assume-screen 5/width, 5/height
  e:&:editor <- new-editor [abcde], 0/left, 5/right
  run [
    render screen, e
  ]
  # still wrap, even though the line would fit. We need room to click on the
  # end of the line
  screen-should-contain [
    .     .
    .abcd↩.
    .e    .
    .     .
  ]
  screen-should-contain-in-color 245/grey [
    .     .
    .    ↩.
    .     .
    .     .
  ]
]

scenario editor-with-empty-text [
  local-scope
  assume-screen 5/width, 5/height
  e:&:editor <- new-editor [], 0/left, 5/right
  run [
    render screen, e
    3:num/raw <- get *e, cursor-row:offset
    4:num/raw <- get *e, cursor-column:offset
  ]
  screen-should-contain [
    .     .
    .     .
    .     .
  ]
  memory-should-contain [
    3 <- 1  # cursor row
    4 <- 0  # cursor column
  ]
]

# just a little color for Mu code

scenario render-colors-comments [
  local-scope
  assume-screen 5/width, 5/height
  s:text <- new [abc
# de
f]
  e:&:editor <- new-editor s, 0/left, 5/right
  run [
    render screen, e
  ]
  screen-should-contain [
    .     .
    .abc  .
    .# de .
    .f    .
    .     .
  ]
  screen-should-contain-in-color 12/lightblue, [
    .     .
    .     .
    .# de .
    .     .
    .     .
  ]
  screen-should-contain-in-color 7/white, [
    .     .
    .abc  .
    .     .
    .f    .
    .     .
  ]
]

after <character-c-received> [
  color <- get-color color, c
]

# so far the previous color is all the information we need; that may change
def get-color color:num, c:char -> color:num [
  local-scope
  load-inputs
  color-is-white?:bool <- equal color, 7/white
  # if color is white and next character is '#', switch color to blue
  {
    break-unless color-is-white?
    starting-comment?:bool <- equal c, 35/#
    break-unless starting-comment?
    trace 90, [app], [switch color back to blue]
    return 12/lightblue
  }
  # if color is blue and next character is newline, switch color to white
  {
    color-is-blue?:bool <- equal color, 12/lightblue
    break-unless color-is-blue?
    ending-comment?:bool <- equal c, 10/newline
    break-unless ending-comment?
    trace 90, [app], [switch color back to white]
    return 7/white
  }
  # if color is white (no comments) and next character is '<', switch color to red
  {
    break-unless color-is-white?
    starting-assignment?:bool <- equal c, 60/<
    break-unless starting-assignment?
    return 1/red
  }
  # if color is red and next character is space, switch color to white
  {
    color-is-red?:bool <- equal color, 1/red
    break-unless color-is-red?
    ending-assignment?:bool <- equal c, 32/space
    break-unless ending-assignment?
    return 7/white
  }
  # otherwise no change
  return color
]

scenario render-colors-assignment [
  local-scope
  assume-screen 8/width, 5/height
  s:text <- new [abc
d <- e
f]
  e:&:editor <- new-editor s, 0/left, 8/right
  run [
    render screen, e
  ]
  screen-should-contain [
    .        .
    .abc     .
    .d <- e  .
    .f       .
    .        .
  ]
  screen-should-contain-in-color 1/red, [
    .        .
    .        .
    .  <-    .
    .        .
    .        .
  ]
]
