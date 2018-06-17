# Mu synchronizes between routines using channels rather than locks, like
# Erlang and Go.
#
# The key property of channels: Writing to a full channel or reading from an
# empty one will put the current routine in 'waiting' state until the
# operation can be completed.
#
# Beware of addresses passed into channels. They can cause race conditions.

scenario channel [
  run [
    local-scope
    source:&:source:num, sink:&:sink:num <- new-channel 3/capacity
    sink <- write sink, 34
    10:num/raw, 11:bool/raw, source <- read source
  ]
  memory-should-contain [
    10 <- 34
    11 <- 0  # read was successful
  ]
]

container channel:_elem [
  lock:bool  # inefficient but simple: serialize all reads as well as writes
  first-full:num  # for write
  first-free:num  # for read
  # A circular buffer contains values from index first-full up to (but not
  # including) index first-free. The reader always modifies it at first-full,
  # while the writer always modifies it at first-free.
  data:&:@:_elem
]

# Since channels have two ends, and since it's an error to use either end from
# multiple routines, let's distinguish the ends.

container source:_elem [
  chan:&:channel:_elem
]

container sink:_elem [
  chan:&:channel:_elem
]

def new-channel capacity:num -> in:&:source:_elem, out:&:sink:_elem [
  local-scope
  load-inputs
  result:&:channel:_elem <- new {(channel _elem): type}
  *result <- put *result, first-full:offset, 0
  *result <- put *result, first-free:offset, 0
  capacity <- add capacity, 1  # unused slot for 'full?' below
  data:&:@:_elem <- new _elem:type, capacity
  *result <- put *result, data:offset, data
  in <- new {(source _elem): type}
  *in <- put *in, chan:offset, result
  out <- new {(sink _elem): type}
  *out <- put *out, chan:offset, result
]

# write a value to a channel
def write out:&:sink:_elem, val:_elem -> out:&:sink:_elem [
  local-scope
  load-inputs
  assert out, [write to null channel]
  chan:&:channel:_elem <- get *out, chan:offset
  <channel-write-initial>
  # block until lock is acquired AND queue has room
  lock:location <- get-location *chan, lock:offset
#?   $print [write], 10/newline
  {
#?     $print [trying to acquire lock for writing], 10/newline
    wait-for-reset-then-set lock
#?     $print [lock acquired for writing], 10/newline
    full?:bool <- channel-full? chan
    break-unless full?
#?     $print [but channel is full; relinquishing lock], 10/newline
    # channel is full; relinquish lock and give a reader the opportunity to
    # create room on it
    reset lock
    current-routine-is-blocked
    switch  # avoid spinlocking
    loop
  }
  current-routine-is-unblocked
#?   $print [performing write], 10/newline
  # store a deep copy of val
  circular-buffer:&:@:_elem <- get *chan, data:offset
  free:num <- get *chan, first-free:offset
  *circular-buffer <- put-index *circular-buffer, free, val
  # mark its slot as filled
  free <- add free, 1
  {
    # wrap free around to 0 if necessary
    len:num <- length *circular-buffer
    at-end?:bool <- greater-or-equal free, len
    break-unless at-end?
    free <- copy 0
  }
  # write back
  *chan <- put *chan, first-free:offset, free
#?   $print [relinquishing lock after writing], 10/newline
  reset lock
]

# read a value from a channel
def read in:&:source:_elem -> result:_elem, eof?:bool, in:&:source:_elem [
  local-scope
  load-inputs
  assert in, [read on null channel]
  eof? <- copy false  # default result
  chan:&:channel:_elem <- get *in, chan:offset
  # block until lock is acquired AND queue has data
  lock:location <- get-location *chan, lock:offset
#?   $print [read], 10/newline
  {
#?     $print [trying to acquire lock for reading], 10/newline
    wait-for-reset-then-set lock
#?     $print [lock acquired for reading], 10/newline
    empty?:bool <- channel-empty? chan
    break-unless empty?
#?     $print [but channel is empty; relinquishing lock], 10/newline
    # channel is empty; relinquish lock and give a writer the opportunity to
    # add to it
    reset lock
    current-routine-is-blocked
    <channel-read-empty>
    switch  # avoid spinlocking
    loop
  }
  current-routine-is-unblocked
  # pull result off
  full:num <- get *chan, first-full:offset
  circular-buffer:&:@:_elem <- get *chan, data:offset
  result <- index *circular-buffer, full
  # clear the slot
  empty:&:_elem <- new _elem:type
  *circular-buffer <- put-index *circular-buffer, full, *empty
  # mark its slot as empty
  full <- add full, 1
  {
    # wrap full around to 0 if necessary
    len:num <- length *circular-buffer
    at-end?:bool <- greater-or-equal full, len
    break-unless at-end?
    full <- copy 0
  }
  # write back
  *chan <- put *chan, first-full:offset, full
#?   $print [relinquishing lock after reading], 10/newline
  reset lock
]

# todo: create a notion of iterator and iterable so we can read/write whole
# aggregates (arrays, lists, ..) of _elems at once.

scenario channel-initialization [
  run [
    local-scope
    source:&:source:num <- new-channel 3/capacity
    chan:&:channel:num <- get *source, chan:offset
    10:num/raw <- get *chan, first-full:offset
    11:num/raw <- get *chan, first-free:offset
  ]
  memory-should-contain [
    10 <- 0  # first-full
    11 <- 0  # first-free
  ]
]

scenario channel-write-increments-free [
  local-scope
  _, sink:&:sink:num <- new-channel 3/capacity
  run [
    sink <- write sink, 34
    chan:&:channel:num <- get *sink, chan:offset
    10:num/raw <- get *chan, first-full:offset
    11:num/raw <- get *chan, first-free:offset
  ]
  memory-should-contain [
    10 <- 0  # first-full
    11 <- 1  # first-free
  ]
]

scenario channel-read-increments-full [
  local-scope
  source:&:source:num, sink:&:sink:num <- new-channel 3/capacity
  sink <- write sink, 34
  run [
    _, _, source <- read source
    chan:&:channel:num <- get *source, chan:offset
    10:num/raw <- get *chan, first-full:offset
    11:num/raw <- get *chan, first-free:offset
  ]
  memory-should-contain [
    10 <- 1  # first-full
    11 <- 1  # first-free
  ]
]

scenario channel-wrap [
  local-scope
  # channel with just 1 slot
  source:&:source:num, sink:&:sink:num <- new-channel 1/capacity
  chan:&:channel:num <- get *source, chan:offset
  # write and read a value
  sink <- write sink, 34
  _, _, source <- read source
  run [
    # first-free will now be 1
    10:num/raw <- get *chan, first-free:offset
    11:num/raw <- get *chan, first-free:offset
    # write second value, verify that first-free wraps
    sink <- write sink, 34
    20:num/raw <- get *chan, first-free:offset
    # read second value, verify that first-full wraps
    _, _, source <- read source
    30:num/raw <- get *chan, first-full:offset
  ]
  memory-should-contain [
    10 <- 1  # first-free after first write
    11 <- 1  # first-full after first read
    20 <- 0  # first-free after second write, wrapped
    30 <- 0  # first-full after second read, wrapped
  ]
]

scenario channel-new-empty-not-full [
  run [
    local-scope
    source:&:source:num <- new-channel 3/capacity
    chan:&:channel:num <- get *source, chan:offset
    10:bool/raw <- channel-empty? chan
    11:bool/raw <- channel-full? chan
  ]
  memory-should-contain [
    10 <- 1  # empty?
    11 <- 0  # full?
  ]
]

scenario channel-write-not-empty [
  local-scope
  source:&:source:num, sink:&:sink:num <- new-channel 3/capacity
  chan:&:channel:num <- get *source, chan:offset
  run [
    sink <- write sink, 34
    10:bool/raw <- channel-empty? chan
    11:bool/raw <- channel-full? chan
  ]
  memory-should-contain [
    10 <- 0  # empty?
    11 <- 0  # full?
  ]
]

scenario channel-write-full [
  local-scope
  source:&:source:num, sink:&:sink:num <- new-channel 1/capacity
  chan:&:channel:num <- get *source, chan:offset
  run [
    sink <- write sink, 34
    10:bool/raw <- channel-empty? chan
    11:bool/raw <- channel-full? chan
  ]
  memory-should-contain [
    10 <- 0  # empty?
    11 <- 1  # full?
  ]
]

scenario channel-read-not-full [
  local-scope
  source:&:source:num, sink:&:sink:num <- new-channel 1/capacity
  chan:&:channel:num <- get *source, chan:offset
  sink <- write sink, 34
  run [
    _, _, source <- read source
    10:bool/raw <- channel-empty? chan
    11:bool/raw <- channel-full? chan
  ]
  memory-should-contain [
    10 <- 1  # empty?
    11 <- 0  # full?
  ]
]

scenario channel-clear [
  local-scope
  # create a channel with a few items
  source:&:source:num, sink:&:sink:num <- new-channel 3/capacity
  chan:&:channel:num <- get *sink, chan:offset
  write sink, 30
  write sink, 31
  write sink, 32
  run [
    clear source
    10:bool/raw <- channel-empty? chan
  ]
  memory-should-contain [
    10 <- 1  # after the call to 'clear', the channel should be empty
  ]
]

def clear in:&:source:_elem -> in:&:source:_elem [
  local-scope
  load-inputs
  chan:&:channel:_elem <- get *in, chan:offset
  {
    empty?:bool <- channel-empty? chan
    break-if empty?
    _, _, in <- read in
    loop
  }
]

## cancelling channels

# every channel comes with a boolean signifying if it's been closed
# initially this boolean is false
container channel:_elem [
  closed?:bool
]

# a channel can be closed from either the source or the sink
# both routines can modify the 'closed?' bit, but they can only ever set it, so this is a benign race
def close x:&:source:_elem -> x:&:source:_elem [
  local-scope
  load-inputs
  chan:&:channel:_elem <- get *x, chan:offset
  *chan <- put *chan, closed?:offset, true
]
def close x:&:sink:_elem -> x:&:sink:_elem [
  local-scope
  load-inputs
  chan:&:channel:_elem <- get *x, chan:offset
  *chan <- put *chan, closed?:offset, true
]

# once a channel is closed from one side, no further operations are expected from that side
# if a channel is closed for reading,
#   no further writes will be let through
# if a channel is closed for writing,
#   future reads continue until the channel empties,
#   then the channel is also closed for reading
after <channel-write-initial> [
  closed?:bool <- get *chan, closed?:offset
  return-if closed?
]
after <channel-read-empty> [
  closed?:bool <- get *chan, closed?:offset
  {
    break-unless closed?
    empty-result:&:_elem <- new _elem:type
    current-routine-is-unblocked
    return *empty-result, true
  }
]

## helpers

# An empty channel has first-free and first-full both at the same value.
def channel-empty? chan:&:channel:_elem -> result:bool [
  local-scope
  load-inputs
  # return chan.first-full == chan.first-free
  full:num <- get *chan, first-full:offset
  free:num <- get *chan, first-free:offset
  result <- equal full, free
]

# A full channel has first-free just before first-full, wasting one slot.
# (Other alternatives: https://www.snellman.net/blog/archive/2016-12-13-ring-buffers)
def channel-full? chan:&:channel:_elem -> result:bool [
  local-scope
  load-inputs
  # tmp = chan.first-free + 1
  tmp:num <- get *chan, first-free:offset
  tmp <- add tmp, 1
  {
    # if tmp == chan.capacity, tmp = 0
    len:num <- capacity chan
    at-end?:bool <- greater-or-equal tmp, len
    break-unless at-end?
    tmp <- copy 0
  }
  # return chan.first-full == tmp
  full:num <- get *chan, first-full:offset
  result <- equal full, tmp
]

def capacity chan:&:channel:_elem -> result:num [
  local-scope
  load-inputs
  q:&:@:_elem <- get *chan, data:offset
  result <- length *q
]

## helpers for channels of characters in particular

def buffer-lines in:&:source:char, buffered-out:&:sink:char -> buffered-out:&:sink:char, in:&:source:char [
  local-scope
  load-inputs
  # repeat forever
  eof?:bool <- copy false
  {
    line:&:buffer:char <- new-buffer 30
    # read characters from 'in' until newline, copy into line
    {
      +next-character
      c:char, eof?:bool, in <- read in
      break-if eof?
      # drop a character on backspace
      {
        # special-case: if it's a backspace
        backspace?:bool <- equal c, 8
        break-unless backspace?
        # drop previous character
        {
          buffer-length:num <- get *line, length:offset
          buffer-empty?:bool <- equal buffer-length, 0
          break-if buffer-empty?
          buffer-length <- subtract buffer-length, 1
          *line <- put *line, length:offset, buffer-length
        }
        # and don't append this one
        loop +next-character
      }
      # append anything else
      line <- append line, c
      line-done?:bool <- equal c, 10/newline
      break-if line-done?
      loop
    }
    # copy line into 'buffered-out'
    i:num <- copy 0
    line-contents:text <- get *line, data:offset
    max:num <- get *line, length:offset
    {
      done?:bool <- greater-or-equal i, max
      break-if done?
      c:char <- index *line-contents, i
      buffered-out <- write buffered-out, c
      i <- add i, 1
      loop
    }
    {
      break-unless eof?
      buffered-out <- close buffered-out
      return
    }
    loop
  }
]

scenario buffer-lines-blocks-until-newline [
  run [
    local-scope
    source:&:source:char, sink:&:sink:char <- new-channel 10/capacity
    _, buffered-stdin:&:sink:char/buffered-stdin <- new-channel 10/capacity
    buffered-chan:&:channel:char <- get *buffered-stdin, chan:offset
    empty?:bool <- channel-empty? buffered-chan
    assert empty?, [ 
F buffer-lines-blocks-until-newline: channel should be empty after init]
    # buffer stdin into buffered-stdin, try to read from buffered-stdin
    buffer-routine:num <- start-running buffer-lines, source, buffered-stdin
    wait-for-routine-to-block buffer-routine
    empty? <- channel-empty? buffered-chan
    assert empty?:bool, [ 
F buffer-lines-blocks-until-newline: channel should be empty after buffer-lines bring-up]
    # write 'a'
    sink <- write sink, 97/a
    restart buffer-routine
    wait-for-routine-to-block buffer-routine
    empty? <- channel-empty? buffered-chan
    assert empty?:bool, [ 
F buffer-lines-blocks-until-newline: channel should be empty after writing 'a']
    # write 'b'
    sink <- write sink, 98/b
    restart buffer-routine
    wait-for-routine-to-block buffer-routine
    empty? <- channel-empty? buffered-chan
    assert empty?:bool, [ 
F buffer-lines-blocks-until-newline: channel should be empty after writing 'b']
    # write newline
    sink <- write sink, 10/newline
    restart buffer-routine
    wait-for-routine-to-block buffer-routine
    empty? <- channel-empty? buffered-chan
    data-emitted?:bool <- not empty?
    assert data-emitted?, [ 
F buffer-lines-blocks-until-newline: channel should contain data after writing newline]
    trace 1, [test], [reached end]
  ]
  trace-should-contain [
    test: reached end
  ]
]

def drain source:&:source:char -> result:text, source:&:source:char [
  local-scope
  load-inputs
  buf:&:buffer:char <- new-buffer 30
  {
    c:char, done?:bool <- read source
    break-if done?
    buf <- append buf, c
    loop
  }
  result <- buffer-to-array buf
]
