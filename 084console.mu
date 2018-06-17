# Wrappers around interaction primitives that take a potentially fake object
# and are thus easier to test.

exclusive-container event [
  text:char
  keycode:num  # keys on keyboard without a unicode representation
  touch:touch-event  # mouse, track ball, etc.
  resize:resize-event
  # update the assume-console handler if you add more variants
]

container touch-event [
  type:num
  row:num
  column:num
]

container resize-event [
  width:num
  height:num
]

container console [
  current-event-index:num
  events:&:@:event
]

def new-fake-console events:&:@:event -> result:&:console [
  local-scope
  load-inputs
  result:&:console <- new console:type
  *result <- put *result, events:offset, events
]

def read-event console:&:console -> result:event, found?:bool, quit?:bool, console:&:console [
  local-scope
  load-inputs
  {
    break-unless console
    current-event-index:num <- get *console, current-event-index:offset
    buf:&:@:event <- get *console, events:offset
    {
      max:num <- length *buf
      done?:bool <- greater-or-equal current-event-index, max
      break-unless done?
      dummy:&:event <- new event:type
      return *dummy, true/found, true/quit
    }
    result <- index *buf, current-event-index
    current-event-index <- add current-event-index, 1
    *console <- put *console, current-event-index:offset, current-event-index
    return result, true/found, false/quit
  }
  switch  # real event source is infrequent; avoid polling it too much
  result:event, found?:bool <- check-for-interaction
  return result, found?, false/quit
]

# variant of read-event for just keyboard events. Discards everything that
# isn't unicode, so no arrow keys, page-up/page-down, etc. But you still get
# newlines, tabs, ctrl-d..
def read-key console:&:console -> result:char, found?:bool, quit?:bool, console:&:console [
  local-scope
  load-inputs
  x:event, found?:bool, quit?:bool, console <- read-event console
  return-if quit?, 0, found?, quit?
  return-unless found?, 0, found?, quit?
  c:char, converted?:bool <- maybe-convert x, text:variant
  return-unless converted?, 0, false/found, false/quit
  return c, true/found, false/quit
]

def send-keys-to-channel console:&:console, chan:&:sink:char, screen:&:screen -> console:&:console, chan:&:sink:char, screen:&:screen [
  local-scope
  load-inputs
  {
    c:char, found?:bool, quit?:bool, console <- read-key console
    loop-unless found?
    break-if quit?
    assert c, [invalid event, expected text]
    screen <- print screen, c
    chan <- write chan, c
    loop
  }
  chan <- close chan
]

def wait-for-event console:&:console -> console:&:console [
  local-scope
  load-inputs
  {
    _, found?:bool <- read-event console
    break-if found?
    switch
    loop
  }
]

def has-more-events? console:&:console -> result:bool [
  local-scope
  load-inputs
  return-if console, false  # fake events are processed as soon as they arrive
  result <- interactions-left?
]
