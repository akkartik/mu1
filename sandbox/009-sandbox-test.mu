## clicking on sandbox results to 'fix' them and turn sandboxes into tests

scenario sandbox-click-on-result-toggles-color-to-green [
  local-scope
  trace-until 100/app  # trace too long
  assume-screen 50/width, 20/height
  # basic recipe
  assume-resources [
    [lesson/recipes.mu] <- [
      |recipe foo [|
      |  reply 4|
      |]|
    ]
  ]
  env:&:environment <- new-programming-environment resources, screen, [foo]
  render-all screen, env, render
  # run it
  assume-console [
    press F4
  ]
  event-loop screen, console, env, resources
  screen-should-contain [
    .                               run (F4)           .
    .                                                  .
    .──────────────────────────────────────────────────.
    .0   edit           copy           delete          .
    .foo                                               .
    .4                                                 .
    .──────────────────────────────────────────────────.
    .                                                  .
  ]
  # click on the '4' in the result
  $clear-trace
  assume-console [
    left-click 5, 21
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # color toggles to green
  screen-should-contain-in-color 2/green, [
    .                                                  .
    .                                                  .
    .                                                  .
    .                                                  .
    .                                                  .
    .4                                                 .
    .                                                  .
  ]
  # don't render entire sandbox side
  check-trace-count-for-label-lesser-than 250, [print-character]  # say 5 sandbox lines
  # cursor should remain unmoved
  run [
    cursor:char <- copy 9251/␣
    print screen, cursor
  ]
  screen-should-contain [
    .                               run (F4)           .
    .␣                                                 .
    .──────────────────────────────────────────────────.
    .0   edit           copy           delete          .
    .foo                                               .
    .4                                                 .
    .──────────────────────────────────────────────────.
    .                                                  .
  ]
  # now change the result
  assume-resources [
    [lesson/recipes.mu] <- [
      |recipe foo [|
      |  reply 3|
      |]|
    ]
  ]
  # then rerun
  assume-console [
    press F4
  ]
  run [
    event-loop screen, console, env, resources
  ]
  # result turns red
  screen-should-contain-in-color 1/red, [
    .                                                  .
    .                                                  .
    .                                                  .
    .                                                  .
    .                                                  .
    .3                                                 .
    .                                                  .
  ]
]

# this requires tracking a couple more things
container sandbox [
  response-starting-row-on-screen:num
  expected-response:text
]

# include expected response when saving or restoring a sandbox
before <end-save-sandbox> [
  {
    expected-response:text <- get *sandbox, expected-response:offset
    break-unless expected-response
    filename <- append filename, [.out]
    resources <- dump resources, filename, expected-response
  }
]

before <end-restore-sandbox> [
  {
    filename <- append filename, [.out]
    contents <- slurp resources, filename
    break-unless contents
    *curr <- put *curr, expected-response:offset, contents
  }
]

# clicks on sandbox responses save it as 'expected'
after <global-touch> [
  # check if it's inside the output of any sandbox
  {
    sandbox-left-margin:num <- get *current-sandbox, left:offset
    click-column:num <- get t, column:offset
    on-sandbox-side?:bool <- greater-or-equal click-column, sandbox-left-margin
    break-unless on-sandbox-side?
    first-sandbox:&:sandbox <- get *env, sandbox:offset
    break-unless first-sandbox
    first-sandbox-begins:num <- get *first-sandbox, starting-row-on-screen:offset
    click-row:num <- get t, row:offset
    below-sandbox-editor?:bool <- greater-or-equal click-row, first-sandbox-begins
    break-unless below-sandbox-editor?
    # identify the sandbox whose output is being clicked on
    sandbox:&:sandbox, sandbox-index:num <- find-click-in-sandbox-output env, click-row
    break-unless sandbox
    # update it
    sandbox <- toggle-expected-response sandbox
    # minimal update to disk
    save-sandbox resources, sandbox, sandbox-index
    # minimal update to screen
    sandbox-right-margin:num <- get *current-sandbox, right:offset
    row:num <- render-sandbox-response screen, sandbox, sandbox-left-margin, sandbox-right-margin
    {
      height:num <- screen-height screen
      at-bottom?:bool <- greater-or-equal row, height
      break-if at-bottom?
      draw-horizontal screen, row, sandbox-left-margin, sandbox-right-margin
    }
    screen <- update-cursor screen, current-sandbox, env
    loop +next-event
  }
]

def find-click-in-sandbox-output env:&:environment, click-row:num -> sandbox:&:sandbox, sandbox-index:num [
  local-scope
  load-inputs
  # assert click-row >= sandbox.starting-row-on-screen
  sandbox:&:sandbox <- get *env, sandbox:offset
  start:num <- get *sandbox, starting-row-on-screen:offset
  clicked-on-sandboxes?:bool <- greater-or-equal click-row, start
  assert clicked-on-sandboxes?, [extract-sandbox called on click to sandbox editor]
  # while click-row < sandbox.next-sandbox.starting-row-on-screen
  sandbox-index <- copy 0
  {
    next-sandbox:&:sandbox <- get *sandbox, next-sandbox:offset
    break-unless next-sandbox
    next-start:num <- get *next-sandbox, starting-row-on-screen:offset
    found?:bool <- lesser-than click-row, next-start
    break-if found?
    sandbox <- copy next-sandbox
    sandbox-index <- add sandbox-index, 1
    loop
  }
  # return sandbox if click is in its output region
  response-starting-row:num <- get *sandbox, response-starting-row-on-screen:offset
  return-unless response-starting-row, null/no-click-in-sandbox-output, 0/sandbox-index
  click-in-response?:bool <- greater-or-equal click-row, response-starting-row
  return-unless click-in-response?, null/no-click-in-sandbox-output, 0/sandbox-index
  return sandbox, sandbox-index
]

def toggle-expected-response sandbox:&:sandbox -> sandbox:&:sandbox [
  local-scope
  load-inputs
  expected-response:text <- get *sandbox, expected-response:offset
  {
    # if expected-response is set, reset
    break-unless expected-response
    *sandbox <- put *sandbox, expected-response:offset, null
  }
  {
    # if not, set expected response to the current response
    break-if expected-response
    response:text <- get *sandbox, response:offset
    *sandbox <- put *sandbox, expected-response:offset, response
  }
]

# when rendering a sandbox, color it in red/green if expected response exists
after <render-sandbox-response> [
  {
    break-unless sandbox-response
    *sandbox <- put *sandbox, response-starting-row-on-screen:offset, row
    row <- render-sandbox-response screen, sandbox, left, right
    jump +render-sandbox-end
  }
]

def render-sandbox-response screen:&:screen, sandbox:&:sandbox, left:num, right:num -> row:num, screen:&:screen [
  local-scope
  load-inputs
  sandbox-response:text <- get *sandbox, response:offset
  expected-response:text <- get *sandbox, expected-response:offset
  row:num <- get *sandbox response-starting-row-on-screen:offset
  {
    break-if expected-response
    row <- render-text screen, sandbox-response, left, right, 245/grey, row
    return
  }
  response-is-expected?:bool <- equal expected-response, sandbox-response
  {
    break-if response-is-expected?
    row <- render-text screen, sandbox-response, left, right, 1/red, row
  }
  {
    break-unless response-is-expected?:bool
    row <- render-text screen, sandbox-response, left, right, 2/green, row
  }
]

before <end-render-sandbox-reset-hidden> [
  *sandbox <- put *sandbox, response-starting-row-on-screen:offset, 0
]
