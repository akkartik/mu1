## running code from the editor and creating sandboxes
#
# Running code in the sandbox editor prepends its contents to a list of
# (non-editable) sandboxes below the editor, showing the result and maybe a
# few other things (later layers).
#
# This layer draws the menubar buttons in non-editable sandboxes but they
# don't do anything yet. Later layers implement each button.

def! main [
  local-scope
  open-console
  clear-screen 0/screen  # non-scrolling app
  env:&:environment <- new-programming-environment 0/filesystem, 0/screen
  env <- restore-sandboxes env, 0/filesystem
  render-all 0/screen, env, render
  event-loop 0/screen, 0/console, env, 0/filesystem
]

container environment [
  sandbox:&:sandbox  # list of sandboxes, from top to bottom. TODO: switch to &:list:sandbox
  render-from:num
  number-of-sandboxes:num
]

after <programming-environment-initialization> [
  *result <- put *result, render-from:offset, -1
]

container sandbox [
  data:text
  response:text
  # coordinates to track clicks
  # constraint: will be 0 for sandboxes at positions before env.render-from
  starting-row-on-screen:num
  code-ending-row-on-screen:num  # past end of code
  screen:&:screen  # prints in the sandbox go here
  next-sandbox:&:sandbox
]

scenario run-and-show-results [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 100/width, 15/height
  # recipe editor is empty
  assume-resources [
  ]
  # sandbox editor contains an instruction without storing outputs
  env:&:environment <- new-programming-environment resources, screen, [divide-with-remainder 11, 3]
  render-all screen, env, render
  # run the code in the editors
  assume-console [
    press F4
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # check that screen prints the results
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊divide-with-remainder 11, 3                      .
    .                                                  ┊3                                                .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
  screen-should-contain-in-color 7/white, [
    .                                                                                                    .
    .                                                                                                    .
    .                                                                                                    .
    .                                                                                                    .
    .                                                   divide-with-remainder 11, 3                      .
    .                                                                                                    .
    .                                                                                                    .
    .                                                                                                    .
    .                                                                                                    .
  ]
  screen-should-contain-in-color 245/grey, [
    .                                                                                                    .
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
    .                                                  ┊                                                 .
    .                                                  ┊3                                                .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
  # sandbox menu in reverse video
  screen-should-contain-in-color 232/black, [
    .                                                                                                    .
    .                                                                                                    .
    .                                                                                                    .
    .                                                   0   edit       copy       to recipe    delete    .
  ]
  # run another command
  assume-console [
    left-click 1, 80
    type [add 2, 2]
    press F4
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # check that screen prints both sandboxes
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊1   edit       copy       to recipe    delete    .
    .                                                  ┊divide-with-remainder 11, 3                      .
    .                                                  ┊3                                                .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
]

after <global-keypress> [
  # F4? load all code and run all sandboxes.
  {
    do-run?:bool <- equal k, 65532/F4
    break-unless do-run?
    screen <- update-status screen, [running...       ], 245/grey
    error?:bool <- run-sandboxes env, resources, screen
    # we could just render-all, but we do some work to minimize the number of prints to screen
    <render-recipe-errors-on-F4>
    screen <- render-sandbox-side screen, env, render
    {
      break-if error?
      screen <- update-status screen, [                 ], 245/grey
    }
    {
      break-unless error?
      <render-sandbox-errors-on-F4>
    }
    screen <- update-cursor screen, recipes, current-sandbox, sandbox-in-focus?, env
    loop +next-event
  }
]

def run-sandboxes env:&:environment, resources:&:resources, screen:&:screen -> errors-found?:bool, env:&:environment, resources:&:resources, screen:&:screen [
  local-scope
  load-ingredients
  errors-found?:bool <- update-recipes env, resources, screen
  jump-if errors-found?, +return
  # check contents of right editor (sandbox)
  <run-sandboxes-begin>
  current-sandbox:&:editor <- get *env, current-sandbox:offset
  {
    sandbox-contents:text <- editor-contents current-sandbox
    break-unless sandbox-contents
    # if contents exist, first save them
    # run them and turn them into a new sandbox
    new-sandbox:&:sandbox <- new sandbox:type
    *new-sandbox <- put *new-sandbox, data:offset, sandbox-contents
    # push to head of sandbox list
    dest:&:sandbox <- get *env, sandbox:offset
    *new-sandbox <- put *new-sandbox, next-sandbox:offset, dest
    *env <- put *env, sandbox:offset, new-sandbox
    # update sandbox count
    sandbox-count:num <- get *env, number-of-sandboxes:offset
    sandbox-count <- add sandbox-count, 1
    *env <- put *env, number-of-sandboxes:offset, sandbox-count
    # clear sandbox editor
    init:&:duplex-list:char <- push 167/§, 0/tail
    *current-sandbox <- put *current-sandbox, data:offset, init
    *current-sandbox <- put *current-sandbox, top-of-screen:offset, init
  }
  # save all sandboxes before running, just in case we die when running
  save-sandboxes env, resources
  # run all sandboxes
  curr:&:sandbox <- get *env, sandbox:offset
  idx:num <- copy 0
  {
    break-unless curr
    curr <- update-sandbox curr, env, idx
    curr <- get *curr, next-sandbox:offset
    idx <- add idx, 1
    loop
  }
  <run-sandboxes-end>
  +return
  {
    break-if resources  # ignore this in tests
    $system [./snapshot_lesson]
  }
]

# load code from disk
# replaced in a later layer (whereupon errors-found? will actually be set)
def update-recipes env:&:environment, resources:&:resources, screen:&:screen -> errors-found?:bool, env:&:environment, resources:&:resources, screen:&:screen [
  local-scope
  load-ingredients
  recipes:&:editor <- get *env, recipes:offset
  in:text <- editor-contents recipes
  resources <- dump resources, [lesson/recipes.mu], in
  reload in
  errors-found? <- copy 0/false
]

# replaced in a later layer
def update-sandbox sandbox:&:sandbox, env:&:environment, idx:num -> sandbox:&:sandbox, env:&:environment [
  local-scope
  load-ingredients
  data:text <- get *sandbox, data:offset
  response:text, _, fake-screen:&:screen <- run-sandboxed data
  *sandbox <- put *sandbox, response:offset, response
  *sandbox <- put *sandbox, screen:offset, fake-screen
]

def update-status screen:&:screen, msg:text, color:num -> screen:&:screen [
  local-scope
  load-ingredients
  screen <- move-cursor screen, 0, 2
  screen <- print screen, msg, color, 238/grey/background
]

def save-sandboxes env:&:environment, resources:&:resources -> resources:&:resources [
  local-scope
  load-ingredients
  current-sandbox:&:editor <- get *env, current-sandbox:offset
  # first clear previous versions, in case we deleted some sandbox
  $system [rm lesson/[0-9]* >/dev/null 2>/dev/null]  # some shells can't handle '>&'
  curr:&:sandbox <- get *env, sandbox:offset
  idx:num <- copy 0
  {
    break-unless curr
    data:text <- get *curr, data:offset
    filename:text <- append [lesson/], idx
    resources <- dump resources, filename, data
    <end-save-sandbox>
    idx <- add idx, 1
    curr <- get *curr, next-sandbox:offset
    loop
  }
]

def! render-sandbox-side screen:&:screen, env:&:environment, render-editor:render-recipe -> screen:&:screen, env:&:environment [
  local-scope
  load-ingredients
  trace 11, [app], [render sandbox side]
  old-top-idx:num <- save-top-idx screen
  current-sandbox:&:editor <- get *env, current-sandbox:offset
  row:num, column:num <- copy 1, 0
  left:num <- get *current-sandbox, left:offset
  right:num <- get *current-sandbox, right:offset
  # render sandbox editor
  render-from:num <- get *env, render-from:offset
  {
    render-current-sandbox?:bool <- equal render-from, -1
    break-unless render-current-sandbox?
    row, column, screen, current-sandbox <- call render-editor, screen, current-sandbox
    clear-screen-from screen, row, column, left, right
    row <- add row, 1
  }
  # render sandboxes
  draw-horizontal screen, row, left, right
  sandbox:&:sandbox <- get *env, sandbox:offset
  row, screen <- render-sandboxes screen, sandbox, left, right, row, render-from
  clear-rest-of-screen screen, row, left, right
  #
  assert-no-scroll screen, old-top-idx
]

def render-sandboxes screen:&:screen, sandbox:&:sandbox, left:num, right:num, row:num, render-from:num, idx:num -> row:num, screen:&:screen, sandbox:&:sandbox [
  local-scope
  load-ingredients
  return-unless sandbox
  screen-height:num <- screen-height screen
  at-bottom?:bool <- greater-or-equal row, screen-height
  return-if at-bottom?
  hidden?:bool <- lesser-than idx, render-from
  {
    break-if hidden?
    # render sandbox menu
    row <- add row, 1
    screen <- move-cursor screen, row, left
    screen <- render-sandbox-menu screen, idx, left, right
    # save menu row so we can detect clicks to it later
    *sandbox <- put *sandbox, starting-row-on-screen:offset, row
    # render sandbox contents
    row <- add row, 1
    screen <- move-cursor screen, row, left
    sandbox-data:text <- get *sandbox, data:offset
    row, screen <- render-code screen, sandbox-data, left, right, row
    *sandbox <- put *sandbox, code-ending-row-on-screen:offset, row
    # render sandbox warnings, screen or response, in that order
    sandbox-response:text <- get *sandbox, response:offset
    <render-sandbox-results>
    {
      sandbox-screen:&:screen <- get *sandbox, screen:offset
      empty-screen?:bool <- fake-screen-is-empty? sandbox-screen
      break-if empty-screen?
      row, screen <- render-screen screen, sandbox-screen, left, right, row
    }
    {
      break-unless empty-screen?
      <render-sandbox-response>
      row, screen <- render-text screen, sandbox-response, left, right, 245/grey, row
    }
    +render-sandbox-end
    at-bottom?:bool <- greater-or-equal row, screen-height
    return-if at-bottom?
    # draw solid line after sandbox
    draw-horizontal screen, row, left, right
  }
  # if hidden, reset row attributes
  {
    break-unless hidden?
    *sandbox <- put *sandbox, starting-row-on-screen:offset, 0
    *sandbox <- put *sandbox, code-ending-row-on-screen:offset, 0
    <end-render-sandbox-reset-hidden>
  }
  # draw next sandbox
  next-sandbox:&:sandbox <- get *sandbox, next-sandbox:offset
  next-idx:num <- add idx, 1
  row, screen <- render-sandboxes screen, next-sandbox, left, right, row, render-from, next-idx
]

def render-sandbox-menu screen:&:screen, sandbox-index:num, left:num, right:num -> screen:&:screen [
  local-scope
  load-ingredients
  move-cursor-to-column screen, left
  edit-button-left:num, edit-button-right:num, copy-button-left:num, copy-button-right:num, recipe-button-left:num, recipe-button-right:num, delete-button-left:num <- sandbox-menu-columns left, right
  print screen, sandbox-index, 232/dark-grey, 245/grey
  start-buttons:num <- subtract edit-button-left, 1
  clear-line-until screen, start-buttons, 245/grey
  print screen, [edit], 232/black, 25/background-blue
  clear-line-until screen, edit-button-right, 25/background-blue
  print screen, [copy], 232/black, 58/background-green
  clear-line-until screen, copy-button-right, 58/background-green
  print screen, [to recipe], 232/black, 94/background-orange
  clear-line-until screen, recipe-button-right, 94/background-orange
  print screen, [delete], 232/black, 52/background-red
  clear-line-until screen, right, 52/background-red
]

# divide up the menu bar for a sandbox into 3 segments, for edit/copy/delete buttons
# delete-button-right == right
# all left/right pairs are inclusive
def sandbox-menu-columns left:num, right:num -> edit-button-left:num, edit-button-right:num, copy-button-left:num, copy-button-right:num, recipe-button-left:num, recipe-button-right:num, delete-button-left:num [
  local-scope
  load-ingredients
  start-buttons:num <- add left, 4/space-for-sandbox-index
  buttons-space:num <- subtract right, start-buttons
  button-width:num <- divide-with-remainder buttons-space, 4  # integer division
  buttons-wide-enough?:bool <- greater-or-equal button-width, 10
  assert buttons-wide-enough?, [sandbox must be at least 40 or so characters wide]
  edit-button-left:num <- copy start-buttons
  copy-button-left:num <- add start-buttons, button-width
  edit-button-right:num <- subtract copy-button-left, 1
  recipe-button-left:num <- add copy-button-left, button-width
  copy-button-right:num <- subtract recipe-button-left, 1
  delete-button-left:num <- subtract right, button-width, -2  # because 'to recipe' is wider than 'delete'
  recipe-button-right:num <- subtract delete-button-left, 1
]

# print a text 's' to 'editor' in 'color' starting at 'row'
# clear rest of last line, move cursor to next line
# like 'render-code' but without syntax-based colorization
def render-text screen:&:screen, s:text, left:num, right:num, color:num, row:num -> row:num, screen:&:screen [
  local-scope
  load-ingredients
  return-unless s
  column:num <- copy left
  screen <- move-cursor screen, row, column
  screen-height:num <- screen-height screen
  i:num <- copy 0
  len:num <- length *s
  {
    +next-character
    done?:bool <- greater-or-equal i, len
    break-if done?
    done? <- greater-or-equal row, screen-height
    break-if done?
    c:char <- index *s, i
    {
      # newline? move to left rather than 0
      newline?:bool <- equal c, 10/newline
      break-unless newline?
      # clear rest of line in this window
      {
        done?:bool <- greater-than column, right
        break-if done?
        space:char <- copy 32/space
        print screen, space
        column <- add column, 1
        loop
      }
      row <- add row, 1
      column <- copy left
      screen <- move-cursor screen, row, column
      i <- add i, 1
      loop +next-character
    }
    {
      # at right? wrap.
      at-right?:bool <- equal column, right
      break-unless at-right?
      # print wrap icon
      wrap-icon:char <- copy 8617/loop-back-to-left
      print screen, wrap-icon, 245/grey
      column <- copy left
      row <- add row, 1
      screen <- move-cursor screen, row, column
      # don't increment i
      loop +next-character
    }
    i <- add i, 1
    print screen, c, color
    column <- add column, 1
    loop
  }
  was-at-left?:bool <- equal column, left
  clear-line-until screen, right
  {
    break-if was-at-left?
    row <- add row, 1
  }
  move-cursor screen, row, left
]

scenario render-text-wraps-barely-long-lines [
  local-scope
  assume-screen 5/width, 5/height
  run [
    render-text screen, [abcde], 0/left, 4/right, 7/white, 1/row
  ]
  screen-should-contain [
    .     .
    .abcd↩.
    .e    .
    .     .
  ]
]

# assumes programming environment has no sandboxes; restores them from previous session
def restore-sandboxes env:&:environment, resources:&:resources -> env:&:environment [
  local-scope
  load-ingredients
  # read all scenarios, pushing them to end of a list of scenarios
  idx:num <- copy 0
  curr:&:sandbox <- copy 0
  prev:&:sandbox <- copy 0
  {
    filename:text <- append [lesson/], idx
    contents:text <- slurp resources, filename
    break-unless contents  # stop at first error; assuming file didn't exist
                           # todo: handle empty sandbox
    # create new sandbox for file
    curr <- new sandbox:type
    *curr <- put *curr, data:offset, contents
    <end-restore-sandbox>
    {
      break-if idx
      *env <- put *env, sandbox:offset, curr
    }
    {
      break-unless idx
      *prev <- put *prev, next-sandbox:offset, curr
    }
    idx <- add idx, 1
    prev <- copy curr
    loop
  }
  # update sandbox count
  *env <- put *env, number-of-sandboxes:offset, idx
]

# print the fake sandbox screen to 'screen' with appropriate delimiters
# leave cursor at start of next line
def render-screen screen:&:screen, sandbox-screen:&:screen, left:num, right:num, row:num -> row:num, screen:&:screen [
  local-scope
  load-ingredients
  return-unless sandbox-screen
  # print 'screen:'
  row <- render-text screen, [screen:], left, right, 245/grey, row
  screen <- move-cursor screen, row, left
  # start printing sandbox-screen
  column:num <- copy left
  s-width:num <- screen-width sandbox-screen
  s-height:num <- screen-height sandbox-screen
  buf:&:@:screen-cell <- get *sandbox-screen, data:offset
  stop-printing:num <- add left, s-width, 3
  max-column:num <- min stop-printing, right
  i:num <- copy 0
  len:num <- length *buf
  screen-height:num <- screen-height screen
  {
    done?:bool <- greater-or-equal i, len
    break-if done?
    done? <- greater-or-equal row, screen-height
    break-if done?
    column <- copy left
    screen <- move-cursor screen, row, column
    # initial leader for each row: two spaces and a '.'
    space:char <- copy 32/space
    print screen, space, 245/grey
    print screen, space, 245/grey
    full-stop:char <- copy 46/period
    print screen, full-stop, 245/grey
    column <- add left, 3
    {
      # print row
      row-done?:bool <- greater-or-equal column, max-column
      break-if row-done?
      curr:screen-cell <- index *buf, i
      c:char <- get curr, contents:offset
      color:num <- get curr, color:offset
      {
        # damp whites down to grey
        white?:bool <- equal color, 7/white
        break-unless white?
        color <- copy 245/grey
      }
      print screen, c, color
      column <- add column, 1
      i <- add i, 1
      loop
    }
    # print final '.'
    print screen, full-stop, 245/grey
    column <- add column, 1
    {
      # clear rest of current line
      line-done?:bool <- greater-than column, right
      break-if line-done?
      print screen, space
      column <- add column, 1
      loop
    }
    row <- add row, 1
    loop
  }
]

scenario run-updates-results [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 100/width, 12/height
  # define a recipe (no indent for the 'add' line below so column numbers are more obvious)
  assume-resources [
    [lesson/recipes.mu] <- [
      ||
      |recipe foo [|
      |  local-scope|
      |  z:num <- add 2, 2|
      |  reply z|
      |]|
    ]
  ]
  # sandbox editor contains an instruction without storing outputs
  env:&:environment <- new-programming-environment resources, screen, [foo]  # contents of sandbox editor
  render-all screen, env, render
  $clear-trace
  # run the code in the editors
  assume-console [
    press F4
  ]
  event-loop screen, console, env, resources
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .recipe foo [                                      ┊─────────────────────────────────────────────────.
    .  local-scope                                     ┊0   edit       copy       to recipe    delete    .
    .  z:num <- add 2, 2                               ┊foo                                              .
    .  reply z                                         ┊4                                                .
    .]                                                 ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊                                                 .
    .                                                  ┊                                                 .
  ]
  # no need to update editor
  trace-should-not-contain [
    app: render recipes
  ]
  # make a change (incrementing one of the args to 'add'), then rerun
  assume-console [
    left-click 4, 28  # one past the value of the second arg
    press backspace
    type [3]
    press F4
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # check that screen updates the result on the right
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .recipe foo [                                      ┊─────────────────────────────────────────────────.
    .  local-scope                                     ┊0   edit       copy       to recipe    delete    .
    .  z:num <- add 2, 3                               ┊foo                                              .
    .  reply z                                         ┊5                                                .
    .]                                                 ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊                                                 .
    .                                                  ┊                                                 .
  ]
]

scenario run-instruction-manages-screen-per-sandbox [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 100/width, 20/height
  # empty recipes
  assume-resources [
  ]
  # sandbox editor contains an instruction
  env:&:environment <- new-programming-environment resources, screen, [print screen, 4]  # contents of sandbox editor
  render-all screen, env, render
  # run the code in the editor
  assume-console [
    press F4
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # check that it prints a little toy screen
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊print screen, 4                                  .
    .                                                  ┊screen:                                          .
    .                                                  ┊  .4                             .               .
    .                                                  ┊  .                              .               .
    .                                                  ┊  .                              .               .
    .                                                  ┊  .                              .               .
    .                                                  ┊  .                              .               .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
]

def editor-contents editor:&:editor -> result:text [
  local-scope
  load-ingredients
  buf:&:buffer:char <- new-buffer 80
  curr:&:duplex-list:char <- get *editor, data:offset
  # skip § sentinel
  assert curr, [editor without data is illegal; must have at least a sentinel]
  curr <- next curr
  return-unless curr, 0
  {
    break-unless curr
    c:char <- get *curr, value:offset
    buf <- append buf, c
    curr <- next curr
    loop
  }
  result <- buffer-to-array buf
]

scenario editor-provides-edited-contents [
  local-scope
  assume-screen 10/width, 5/height
  e:&:editor <- new-editor [abc], 0/left, 10/right
  assume-console [
    left-click 1, 2
    type [def]
  ]
  run [
    editor-event-loop screen, console, e
    s:text <- editor-contents e
    1:@:char/raw <- copy *s
  ]
  memory-should-contain [
    1:array:character <- [abdefc]
  ]
]

# keep the bottom of recipes from scrolling off the screen

scenario scrolling-down-past-bottom-of-recipe-editor [
  local-scope
  trace-until 100/app
  assume-screen 100/width, 10/height
  assume-resources [
  ]
  env:&:environment <- new-programming-environment resources, screen, []
  render-all screen, env, render
  assume-console [
    press enter
    press down-arrow
  ]
  event-loop screen, console, env, resources
  # no scroll
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊                                                 .
    .                                                  ┊                                                 .
  ]
]

scenario cursor-down-in-recipe-editor [
  local-scope
  trace-until 100/app
  assume-screen 100/width, 10/height
  assume-resources [
  ]
  env:&:environment <- new-programming-environment resources, screen, []
  render-all screen, env, render
  assume-console [
    press enter
    press up-arrow
    press down-arrow  # while cursor isn't at bottom
  ]
  event-loop screen, console, env, resources
  cursor:char <- copy 9251/␣
  print screen, cursor
  # cursor moves back to bottom
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .␣                                                 ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊                                                 .
    .                                                  ┊                                                 .
  ]
]

# we'll not use the recipe-editor's 'bottom' element directly, because later
# layers will add other stuff to the left side below the editor (error messages)

container environment [
  recipe-bottom:num
]

after <render-recipe-components-end> [
  *env <- put *env, recipe-bottom:offset, row
]

after <global-keypress> [
  {
    break-if sandbox-in-focus?
    down-arrow?:bool <- equal k, 65516/down-arrow
    break-unless down-arrow?
    recipe-editor:&:editor <- get *env, recipes:offset
    recipe-cursor-row:num <- get *recipe-editor, cursor-row:offset
    recipe-editor-bottom:num <- get *recipe-editor, bottom:offset
    at-bottom-of-editor?:bool <- greater-or-equal recipe-cursor-row, recipe-editor-bottom
    break-unless at-bottom-of-editor?
    more-to-scroll?:bool <- more-to-scroll? env, screen
    break-if more-to-scroll?
    loop +next-event
  }
  {
    break-if sandbox-in-focus?
    page-down?:bool <- equal k, 65518/page-down
    break-unless page-down?
    more-to-scroll?:bool <- more-to-scroll? env, screen
    break-if more-to-scroll?
    loop +next-event
  }
]

after <global-type> [
  {
    break-if sandbox-in-focus?
    page-down?:bool <- equal k, 6/ctrl-f
    break-unless page-down?
    more-to-scroll?:bool <- more-to-scroll? env, screen
    break-if more-to-scroll?
    loop +next-event
  }
]

def more-to-scroll? env:&:environment, screen:&:screen -> result:bool [
  local-scope
  load-ingredients
  recipe-bottom:num <- get *env, recipe-bottom:offset
  height:num <- screen-height screen
  result <- greater-or-equal recipe-bottom, height
]

scenario scrolling-down-past-bottom-of-recipe-editor-2 [
  local-scope
  trace-until 100/app
  assume-screen 100/width, 10/height
  assume-resources [
  ]
  env:&:environment <- new-programming-environment resources, screen, []
  render-all screen, env, render
  assume-console [
    # add a line
    press enter
    # cursor back to top line
    press up-arrow
    # try to scroll
    press page-down  # or ctrl-f
  ]
  event-loop screen, console, env, resources
  # no scroll, and cursor remains at top line
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊                                                 .
    .                                                  ┊                                                 .
  ]
]

scenario scrolling-down-past-bottom-of-recipe-editor-3 [
  local-scope
  trace-until 100/app
  assume-screen 100/width, 10/height
  assume-resources [
  ]
  env:&:environment <- new-programming-environment resources, screen, [ab
cd]
  render-all screen, env, render
  assume-console [
    # add a line
    press enter
    # switch to sandbox
    press ctrl-n
    # move cursor
    press down-arrow
  ]
  event-loop screen, console, env, resources
  cursor:char <- copy 9251/␣
  print screen, cursor
  # no scroll on recipe side, cursor moves on sandbox side
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊ab                                               .
    .                                                  ┊␣d                                               .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
]

# scrolling through sandboxes

scenario scrolling-down-past-bottom-of-sandbox-editor [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 100/width, 10/height
  # initialize
  assume-resources [
  ]
  env:&:environment <- new-programming-environment resources, screen, [add 2, 2]
  render-all screen, env, render
  assume-console [
    # create a sandbox
    press F4
  ]
  event-loop screen, console, env, resources
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
  ]
  # switch to sandbox window and hit 'page-down'
  assume-console [
    press ctrl-n
    press page-down
  ]
  run [
    event-loop screen, console, env, resources
    cursor:char <- copy 9251/␣
    print screen, cursor
  ]
  # sandbox editor hidden; first sandbox displayed
  # cursor moves to first sandbox
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊␣   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
  ]
  # hit 'page-up'
  assume-console [
    press page-up
  ]
  run [
    event-loop screen, console, env, resources
    cursor:char <- copy 9251/␣
    print screen, cursor
  ]
  # sandbox editor displays again, cursor is in editor
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊␣                                                .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
  ]
]

# page-down on sandbox side updates render-from to scroll sandboxes
after <global-keypress> [
  {
    break-unless sandbox-in-focus?
    page-down?:bool <- equal k, 65518/page-down
    break-unless page-down?
    sandbox:&:sandbox <- get *env, sandbox:offset
    break-unless sandbox
    # slide down if possible
    {
      render-from:num <- get *env, render-from:offset
      number-of-sandboxes:num <- get *env, number-of-sandboxes:offset
      max:num <- subtract number-of-sandboxes, 1
      at-end?:bool <- greater-or-equal render-from, max
      loop-if at-end?, +next-event  # render nothing
      render-from <- add render-from, 1
      *env <- put *env, render-from:offset, render-from
    }
    screen <- render-sandbox-side screen, env, render
    screen <- update-cursor screen, recipes, current-sandbox, sandbox-in-focus?, env
    loop +next-event
  }
]

# update-cursor takes render-from into account
after <update-cursor-special-cases> [
  {
    break-unless sandbox-in-focus?
    render-from:num <- get *env, render-from:offset
    scrolling?:bool <- greater-or-equal render-from, 0
    break-unless scrolling?
    cursor-column:num <- get *current-sandbox, left:offset
    screen <- move-cursor screen, 2/row, cursor-column  # highlighted sandbox will always start at row 2
    return
  }
]

# 'page-up' on sandbox side is like 'page-down': updates render-from when necessary
after <global-keypress> [
  {
    break-unless sandbox-in-focus?
    page-up?:bool <- equal k, 65519/page-up
    break-unless page-up?
    render-from:num <- get *env, render-from:offset
    at-beginning?:bool <- equal render-from, -1
    break-if at-beginning?
    render-from <- subtract render-from, 1
    *env <- put *env, render-from:offset, render-from
    screen <- render-sandbox-side screen, env, render
    screen <- update-cursor screen, recipes, current-sandbox, sandbox-in-focus?, env
    loop +next-event
  }
]

# sandbox belonging to 'env' whose next-sandbox is 'in'
# return 0 if there's no such sandbox, either because 'in' doesn't exist in 'env', or because it's the first sandbox
def previous-sandbox env:&:environment, in:&:sandbox -> out:&:sandbox [
  local-scope
  load-ingredients
  curr:&:sandbox <- get *env, sandbox:offset
  return-unless curr, 0/nil
  next:&:sandbox <- get *curr, next-sandbox:offset
  {
    return-unless next, 0/nil
    found?:bool <- equal next, in
    break-if found?
    curr <- copy next
    next <- get *curr, next-sandbox:offset
    loop
  }
  return curr
]

scenario scrolling-down-past-bottom-on-recipe-side [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 100/width, 10/height
  # initialize sandbox side and create a sandbox
  assume-resources [
    [lesson/recipes.mu] <- [
      ||  # file containing just a newline
    ]
  ]
  # create a sandbox
  env:&:environment <- new-programming-environment resources, screen, [add 2, 2]
  render-all screen, env, render
  assume-console [
    press F4
  ]
  event-loop screen, console, env, resources
  # hit 'down' in recipe editor
  assume-console [
    press page-down
  ]
  run [
    event-loop screen, console, env, resources
    cursor:char <- copy 9251/␣
    print screen, cursor
  ]
  # cursor doesn't move when the end is already on-screen
  screen-should-contain [
    .                                                                                 run (F4)           .
    .␣                                                 ┊                                                 .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
  ]
]

scenario scrolling-through-multiple-sandboxes [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 100/width, 10/height
  # initialize environment
  assume-resources [
  ]
  env:&:environment <- new-programming-environment resources, screen, []
  render-all screen, env, render
  # create 2 sandboxes
  assume-console [
    press ctrl-n
    type [add 2, 2]
    press F4
    type [add 1, 1]
    press F4
  ]
  event-loop screen, console, env, resources
  cursor:char <- copy 9251/␣
  print screen, cursor
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊␣                                                .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊1   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
  ]
  # hit 'page-down'
  assume-console [
    press page-down
  ]
  run [
    event-loop screen, console, env, resources
    cursor:char <- copy 9251/␣
    print screen, cursor
  ]
  # sandbox editor hidden; first sandbox displayed
  # cursor moves to first sandbox
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊␣   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊1   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
  ]
  # hit 'page-down' again
  assume-console [
    press page-down
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # just second sandbox displayed
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊1   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
  # hit 'page-down' again
  assume-console [
    press page-down
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # no change
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊1   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
  # hit 'page-up'
  assume-console [
    press page-up
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # back to displaying both sandboxes without editor
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊1   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
  ]
  # hit 'page-up' again
  assume-console [
    press page-up
  ]
  run [
    event-loop screen, console, env, resources
    cursor:char <- copy 9251/␣
    print screen, cursor
  ]
  # back to displaying both sandboxes as well as editor
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊␣                                                .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊1   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
  ]
  # hit 'page-up' again
  assume-console [
    press page-up
  ]
  run [
    event-loop screen, console, env, resources
    cursor:char <- copy 9251/␣
    print screen, cursor
  ]
  # no change
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊␣                                                .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊1   edit       copy       to recipe    delete    .
    .                                                  ┊add 2, 2                                         .
    .                                                  ┊4                                                .
  ]
]

scenario scrolling-manages-sandbox-index-correctly [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 100/width, 10/height
  # initialize environment
  assume-resources [
  ]
  env:&:environment <- new-programming-environment resources, screen, []
  render-all screen, env, render
  # create a sandbox
  assume-console [
    press ctrl-n
    type [add 1, 1]
    press F4
  ]
  event-loop screen, console, env, resources
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
  # hit 'page-down' and 'page-up' a couple of times. sandbox index should be stable
  assume-console [
    press page-down
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # sandbox editor hidden; first sandbox displayed
  # cursor moves to first sandbox
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
  # hit 'page-up' again
  assume-console [
    press page-up
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # back to displaying both sandboxes as well as editor
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊                                                 .
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊─────────────────────────────────────────────────.
    .                                                  ┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
  # hit 'page-down'
  assume-console [
    press page-down
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # sandbox editor hidden; first sandbox displayed
  # cursor moves to first sandbox
  screen-should-contain [
    .                                                                                 run (F4)           .
    .                                                  ┊─────────────────────────────────────────────────.
    .┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┊0   edit       copy       to recipe    delete    .
    .                                                  ┊add 1, 1                                         .
    .                                                  ┊2                                                .
    .                                                  ┊─────────────────────────────────────────────────.
    .                                                  ┊                                                 .
  ]
]
