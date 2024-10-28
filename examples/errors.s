// All the possible AST errors collected

push main
jmp

// "Extra characters after blob directive"
// .ascii "Hello world!", 0

// Warning
.u64 0xff

.ascii "Hello world\0"

// Unknown directive
// .foobar

// Unexpected characters found
// ???

.const BITS (1 << NBITS) - 1

main:
  // die
  cli
  hlt
