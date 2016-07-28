# new type to help incrementally read texts (arrays of characters)
container stream [
  index:number
  data:address:array:character
]

def new-stream s:address:array:character -> result:address:stream [
  local-scope
  load-ingredients
  result <- new stream:type
  *result <- put *result, index:offset, 0
  *result <- put *result, data:offset, s
]

def rewind in:address:stream -> in:address:stream [
  local-scope
  load-ingredients
  *in <- put *in, index:offset, 0
]

def read in:address:stream -> result:character, in:address:stream [
  local-scope
  load-ingredients
  idx:number <- get *in, index:offset
  s:address:array:character <- get *in, data:offset
  len:number <- length *s
  at-end?:boolean <- greater-or-equal idx len
  return-if at-end?, 0/nul, in
  result <- index *s, idx
  idx <- add idx, 1
  *in <- put *in, index:offset, idx
]

def peek in:address:stream -> result:character [
  local-scope
  load-ingredients
  idx:number <- get *in, index:offset
  s:address:array:character <- get *in, data:offset
  len:number <- length *s
  at-end?:boolean <- greater-or-equal idx len
  return-if at-end?, 0/nul
  result <- index *s, idx
]

def read-line in:address:stream -> result:address:array:character, in:address:stream [
  local-scope
  load-ingredients
  idx:number <- get *in, index:offset
  s:address:array:character <- get *in, data:offset
  next-idx:number <- find-next s, 10/newline, idx
  result <- copy-range s, idx, next-idx
  idx <- add next-idx, 1  # skip newline
  # write back
  *in <- put *in, index:offset, idx
]

def end-of-stream? in:address:stream -> result:boolean [
  local-scope
  load-ingredients
  idx:number <- get *in, index:offset
  s:address:array:character <- get *in, data:offset
  len:number <- length *s
  result <- greater-or-equal idx, len
]
