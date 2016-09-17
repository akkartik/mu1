## experimental compiler to translate programs written in a generic
## expression-oriented language called 'lambda' into mu

scenario convert-lambda [
  run [
    local-scope
    1:text/raw <- lambda-to-mu [(add a (multiply b c))]
    2:@:char/raw <- copy *1:text/raw
  ]
  memory-should-contain [
    2:@:char <- [t1 <- multiply b c
result <- add a t1]
  ]
]

def lambda-to-mu in:text -> out:text [
  local-scope
  load-ingredients
  out <- copy 0
  cells:&:cell <- parse in
  out <- to-mu cells
]

# 'parse' will turn lambda expressions into trees made of cells
exclusive-container cell [
  atom:text
  pair:pair
]

# printed below as < first | rest >
container pair [
  first:&:cell
  rest:&:cell
]

def new-atom name:text -> result:&:cell [
  local-scope
  load-ingredients
  result <- new cell:type
  *result <- merge 0/tag:atom, name
]

def new-pair a:&:cell, b:&:cell -> result:&:cell [
  local-scope
  load-ingredients
  result <- new cell:type
  *result <- merge 1/tag:pair, a/first, b/rest
]

def is-atom? x:&:cell -> result:boolean [
  local-scope
  load-ingredients
  reply-unless x, 0/false
  _, result <- maybe-convert *x, atom:variant
]

def is-pair? x:&:cell -> result:boolean [
  local-scope
  load-ingredients
  reply-unless x, 0/false
  _, result <- maybe-convert *x, pair:variant
]

scenario atom-is-not-pair [
  local-scope
  s:text <- new [a]
  x:&:cell <- new-atom s
  10:boolean/raw <- is-atom? x
  11:boolean/raw <- is-pair? x
  memory-should-contain [
    10 <- 1
    11 <- 0
  ]
]

scenario pair-is-not-atom [
  local-scope
  # construct (a . nil)
  s:text <- new [a]
  x:&:cell <- new-atom s
  y:&:cell <- new-pair x, 0/nil
  10:boolean/raw <- is-atom? y
  11:boolean/raw <- is-pair? y
  memory-should-contain [
    10 <- 0
    11 <- 1
  ]
]

def atom-match? x:&:cell, pat:text -> result:boolean [
  local-scope
  load-ingredients
  s:text, is-atom?:boolean <- maybe-convert *x, atom:variant
  reply-unless is-atom?, 0/false
  result <- equal pat, s
]

scenario atom-match [
  local-scope
  x:&:cell <- new-atom [abc]
  10:boolean/raw <- atom-match? x, [abc]
  memory-should-contain [
    10 <- 1
  ]
]

def first x:&:cell -> result:&:cell [
  local-scope
  load-ingredients
  pair:pair, pair?:boolean <- maybe-convert *x, pair:variant
  reply-unless pair?, 0/nil
  result <- get pair, first:offset
]

def rest x:&:cell -> result:&:cell [
  local-scope
  load-ingredients
  pair:pair, pair?:boolean <- maybe-convert *x, pair:variant
  reply-unless pair?, 0/nil
  result <- get pair, rest:offset
]

def set-first base:&:cell, new-first:&:cell -> base:&:cell [
  local-scope
  load-ingredients
  pair:pair, is-pair?:boolean <- maybe-convert *base, pair:variant
  reply-unless is-pair?
  pair <- put pair, first:offset, new-first
  *base <- merge 1/pair, pair
]

def set-rest base:&:cell, new-rest:&:cell -> base:&:cell [
  local-scope
  load-ingredients
  pair:pair, is-pair?:boolean <- maybe-convert *base, pair:variant
  reply-unless is-pair?
  pair <- put pair, rest:offset, new-rest
  *base <- merge 1/pair, pair
]

scenario cell-operations-on-atom [
  local-scope
  s:text <- new [a]
  x:&:cell <- new-atom s
  10:&:cell/raw <- first x
  11:&:cell/raw <- rest x
  memory-should-contain [
    10 <- 0  # first is nil
    11 <- 0  # rest is nil
  ]
]

scenario cell-operations-on-pair [
  local-scope
  # construct (a . nil)
  s:text <- new [a]
  x:&:cell <- new-atom s
  y:&:cell <- new-pair x, 0/nil
  x2:&:cell <- first y
  10:boolean/raw <- equal x, x2
  11:&:cell/raw <- rest y
  memory-should-contain [
    10 <- 1  # first is correct
    11 <- 0  # rest is nil
  ]
]

## convert lambda text to a tree of cells

def parse in:text -> out:&:cell [
  local-scope
  load-ingredients
  s:&:stream:char <- new-stream in
  out, s <- parse s
  trace 2, [app/parse], out
]

def parse in:&:stream:char -> out:&:cell, in:&:stream:char [
  local-scope
  load-ingredients
  # skip whitespace
  in <- skip-whitespace in
  c:char, eof?:boolean <- peek in
  reply-if eof?, 0/nil
  pair?:boolean <- equal c, 40/open-paren
  {
    break-if pair?
    # atom
    b:&:buffer <- new-buffer 30
    {
      done?:boolean <- end-of-stream? in
      break-if done?
      # stop before close paren or space
      c:char <- peek in
      done? <- equal c, 41/close-paren
      break-if done?
      done? <- space? c
      break-if done?
      c <- read in
      b <- append b, c
      loop
    }
    s:text <- buffer-to-array b
    out <- new-atom s
  }
  {
    break-unless pair?
    # pair
    read in  # skip the open-paren
    out <- new cell:type  # start out with nil
    # read in first element of pair
    {
      end?:boolean <- end-of-stream? in
      not-end?:boolean <- not end?
      assert not-end?, [unbalanced '(' in expression]
      c <- peek in
      close-paren?:boolean <- equal c, 41/close-paren
      break-if close-paren?
      first:&:cell, in <- parse in
      *out <- merge 1/pair, first, 0/nil
    }
    # read in any remaining elements
    curr:&:cell <- copy out
    {
      in <- skip-whitespace in
      end?:boolean <- end-of-stream? in
      not-end?:boolean <- not end?
      assert not-end?, [unbalanced '(' in expression]
      # termination check: ')'
      c <- peek in
      {
        close-paren?:boolean <- equal c, 41/close-paren
        break-unless close-paren?
        read in  # skip ')'
        break +end-pair:label
      }
      # still here? read next element of pair
      next:&:cell, in <- parse in
      is-dot?:boolean <- atom-match? next, [.]
      {
        break-if is-dot?
        next-curr:&:cell <- new-pair next, 0/nil
        curr <- set-rest curr, next-curr
        curr <- rest curr
      }
      {
        break-unless is-dot?
        # deal with dotted pair
        in <- skip-whitespace in
        c <- peek in
        not-close-paren?:boolean <- not-equal c, 41/close-paren
        assert not-close-paren?, [')' cannot immediately follow '.']
        final:&:cell <- parse in
        curr <- set-rest curr, final
        # we're not gonna update curr, so better make sure the next iteration
        # is going to end the pair
        in <- skip-whitespace in
        c <- peek in
        close-paren?:boolean <- equal c, 41/close-paren
        assert close-paren?, ['.' must be followed by exactly one expression before ')']
      }
      loop
    }
    +end-pair
  }
]

def skip-whitespace in:&:stream:char -> in:&:stream:char [
  local-scope
  load-ingredients
  {
    done?:boolean <- end-of-stream? in
    reply-if done?, 0/null
    c:char <- peek in
    space?:boolean <- space? c
    break-unless space?
    read in  # skip
    loop
  }
]

def to-text x:&:cell -> out:text [
  local-scope
  load-ingredients
  buf:&:buffer <- new-buffer 30
  buf <- to-buffer x, buf
  out <- buffer-to-array buf
]

def to-buffer x:&:cell, buf:&:buffer -> buf:&:buffer [
  local-scope
  load-ingredients
  # base case: empty cell
  {
    break-if x
    buf <- append buf, [<>]
    reply
  }
  # base case: atom
  {
    s:text, atom?:boolean <- maybe-convert *x, atom:variant
    break-unless atom?
    buf <- append buf, s
    reply
  }
  # recursive case: pair
  buf <- append buf, [< ]
  first:&:cell <- first x
  buf <- to-buffer first, buf
  buf <- append buf, [ | ]
  rest:&:cell <- rest x
  buf <- to-buffer rest, buf
  buf <- append buf, [ >]
]

scenario parse-single-letter-atom [
  local-scope
  s:text <- new [a]
  x:&:cell <- parse s
  s2:text, 10:boolean/raw <- maybe-convert *x, atom:variant
  11:@:char/raw <- copy *s2
  memory-should-contain [
    10 <- 1  # parse result is an atom
    11:@:char <- [a]
  ]
]

scenario parse-atom [
  local-scope
  s:text <- new [abc]
  x:&:cell <- parse s
  s2:text, 10:boolean/raw <- maybe-convert *x, atom:variant
  11:@:char/raw <- copy *s2
  memory-should-contain [
    10 <- 1  # parse result is an atom
    11:@:char <- [abc]
  ]
]

scenario parse-list-of-two-atoms [
  local-scope
  s:text <- new [(abc def)]
  x:&:cell <- parse s
  trace-should-contain [
    app/parse: < abc | < def | <> > >
  ]
  10:boolean/raw <- is-pair? x
  x1:&:cell <- first x
  x2:&:cell <- rest x
  s1:text, 11:boolean/raw <- maybe-convert *x1, atom:variant
  12:boolean/raw <- is-pair? x2
  x3:&:cell <- first x2
  s2:text, 13:boolean/raw <- maybe-convert *x3, atom:variant
  14:&:cell/raw <- rest x2
  20:@:char/raw <- copy *s1
  30:@:char/raw <- copy *s2
  memory-should-contain [
    10 <- 1  # parse result is a pair
    11 <- 1  # result.first is an atom
    12 <- 1  # result.rest is a pair
    13 <- 1  # result.rest.first is an atom
    14 <- 0  # result.rest.rest is nil
    20:@:char <- [abc]  # result.first
    30:@:char <- [def]  # result.rest.first
  ]
]

scenario parse-list-with-extra-spaces [
  local-scope
  s:text <- new [ ( abc  def ) ]  # extra spaces
  x:&:cell <- parse s
  trace-should-contain [
    app/parse: < abc | < def | <> > >
  ]
  10:boolean/raw <- is-pair? x
  x1:&:cell <- first x
  x2:&:cell <- rest x
  s1:text, 11:boolean/raw <- maybe-convert *x1, atom:variant
  12:boolean/raw <- is-pair? x2
  x3:&:cell <- first x2
  s2:text, 13:boolean/raw <- maybe-convert *x3, atom:variant
  14:&:cell/raw <- rest x2
  20:@:char/raw <- copy *s1
  30:@:char/raw <- copy *s2
  memory-should-contain [
    10 <- 1  # parse result is a pair
    11 <- 1  # result.first is an atom
    12 <- 1  # result.rest is a pair
    13 <- 1  # result.rest.first is an atom
    14 <- 0  # result.rest.rest is nil
    20:@:char <- [abc]  # result.first
    30:@:char <- [def]  # result.rest.first
  ]
]

scenario parse-list-of-more-than-two-atoms [
  local-scope
  s:text <- new [(abc def ghi)]
  x:&:cell <- parse s
  trace-should-contain [
    app/parse: < abc | < def | < ghi | <> > > >
  ]
  10:boolean/raw <- is-pair? x
  x1:&:cell <- first x
  x2:&:cell <- rest x
  s1:text, 11:boolean/raw <- maybe-convert *x1, atom:variant
  12:boolean/raw <- is-pair? x2
  x3:&:cell <- first x2
  s2:text, 13:boolean/raw <- maybe-convert *x3, atom:variant
  x4:&:cell <- rest x2
  14:boolean/raw <- is-pair? x4
  x5:&:cell <- first x4
  s3:text, 15:boolean/raw <- maybe-convert *x5, atom:variant
  16:&:cell/raw <- rest x4
  20:@:char/raw <- copy *s1
  30:@:char/raw <- copy *s2
  40:@:char/raw <- copy *s3
  memory-should-contain [
    10 <- 1  # parse result is a pair
    11 <- 1  # result.first is an atom
    12 <- 1  # result.rest is a pair
    13 <- 1  # result.rest.first is an atom
    14 <- 1  # result.rest.rest is a pair
    15 <- 1  # result.rest.rest.first is an atom
    16 <- 0  # result.rest.rest.rest is nil
    20:@:char <- [abc]  # result.first
    30:@:char <- [def]  # result.rest.first
    40:@:char <- [ghi]  # result.rest.rest
  ]
]

scenario parse-nested-list [
  local-scope
  s:text <- new [((abc))]
  x:&:cell <- parse s
  trace-should-contain [
    app/parse: < < abc | <> > | <> >
  ]
  10:boolean/raw <- is-pair? x
  x1:&:cell <- first x
  11:boolean/raw <- is-pair? x
  x2:&:cell <- first x1
  s1:text, 12:boolean/raw <- maybe-convert *x2, atom:variant
  13:&:cell/raw <- rest x1
  14:&:cell/raw <- rest x
  20:@:char/raw <- copy *s1
  memory-should-contain [
    10 <- 1  # parse result is a pair
    11 <- 1  # result.first is a pair
    12 <- 1  # result.first.first is an atom
    13 <- 0  # result.first.rest is nil
    14 <- 0  # result.rest is nil
    20:@:char <- [abc]  # result.first.first
  ]
]

scenario parse-nested-list-2 [
  local-scope
  s:text <- new [((abc) def)]
  x:&:cell <- parse s
  trace-should-contain [
    app/parse: < < abc | <> > | < def | <> > >
  ]
  10:boolean/raw <- is-pair? x
  x1:&:cell <- first x
  11:boolean/raw <- is-pair? x
  x2:&:cell <- first x1
  s1:text, 12:boolean/raw <- maybe-convert *x2, atom:variant
  13:&:cell/raw <- rest x1
  x3:&:cell <- rest x
  x4:&:cell <- first x3
  s2:text, 14:boolean/raw <- maybe-convert *x4, atom:variant
  15:&:cell/raw <- rest x3
  20:@:char/raw <- copy *s1
  30:@:char/raw <- copy *s2
  memory-should-contain [
    10 <- 1  # parse result is a pair
    11 <- 1  # result.first is a pair
    12 <- 1  # result.first.first is an atom
    13 <- 0  # result.first.rest is nil
    14 <- 1  # result.rest.first is an atom
    15 <- 0  # result.rest.rest is nil
    20:@:char <- [abc]  # result.first.first
    30:@:char <- [def]  # result.rest.first
  ]
]

# todo: uncomment these tests after we figure out how to continue tests after
# assertion failures
#? scenario parse-error [
#?   local-scope
#?   s:text <- new [(]
#? #?   hide-errors
#?   x:&:cell <- parse s
#? #?   show-errors
#?   trace-should-contain [
#?     error: unbalanced '(' in expression
#?   ]
#? ]
#? 
#? scenario parse-error-after-element [
#?   local-scope
#?   s:text <- new [(abc]
#? #?   hide-errors
#?   x:&:cell <- parse s
#? #?   show-errors
#?   trace-should-contain [
#?     error: unbalanced '(' in expression
#?   ]
#? ]

scenario parse-dotted-list-of-two-atoms [
  local-scope
  s:text <- new [(abc . def)]
  x:&:cell <- parse s
  trace-should-contain [
    app/parse: < abc | def >
  ]
  10:boolean/raw <- is-pair? x
  x1:&:cell <- first x
  x2:&:cell <- rest x
  s1:text, 11:boolean/raw <- maybe-convert *x1, atom:variant
  s2:text, 12:boolean/raw <- maybe-convert *x2, atom:variant
  20:@:char/raw <- copy *s1
  30:@:char/raw <- copy *s2
  memory-should-contain [
    # parses to < abc | def >
    10 <- 1  # parse result is a pair
    11 <- 1  # result.first is an atom
    12 <- 1  # result.rest is an atom
    20:@:char <- [abc]  # result.first
    30:@:char <- [def]  # result.rest
  ]
]

scenario parse-dotted-list-of-more-than-two-atoms [
  local-scope
  s:text <- new [(abc def . ghi)]
  x:&:cell <- parse s
  trace-should-contain [
    app/parse: < abc | < def | ghi > >
  ]
  10:boolean/raw <- is-pair? x
  x1:&:cell <- first x
  x2:&:cell <- rest x
  s1:text, 11:boolean/raw <- maybe-convert *x1, atom:variant
  12:boolean/raw <- is-pair? x2
  x3:&:cell <- first x2
  s2:text, 13:boolean/raw <- maybe-convert *x3, atom:variant
  x4:&:cell <- rest x2
  s3:text, 14:boolean/raw <- maybe-convert *x4, atom:variant
  20:@:char/raw <- copy *s1
  30:@:char/raw <- copy *s2
  40:@:char/raw <- copy *s3
  memory-should-contain [
    10 <- 1  # parse result is a pair
    11 <- 1  # result.first is an atom
    12 <- 1  # result.rest is a pair
    13 <- 1  # result.rest.first is an atom
    14 <- 1  # result.rest.rest is an atom
    20:@:char <- [abc]  # result.first
    30:@:char <- [def]  # result.rest.first
    40:@:char <- [ghi]  # result.rest.rest
  ]
]

## convert tree of cells to mu text

def to-mu in:&:cell -> out:text [
  local-scope
  load-ingredients
  buf:&:buffer <- new-buffer 30
  buf <- to-mu in, buf
  out <- buffer-to-array buf
]

def to-mu in:&:cell, buf:&:buffer -> buf:&:buffer, result-name:text [
  local-scope
  load-ingredients
  # null cell? no change.
  # pair with all atoms? gensym a new variable
  # pair containing other pairs? recurse
  result-name <- copy 0
]
