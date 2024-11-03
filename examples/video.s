.const INT_TIMER1 0x01

push handle_timer1
push INT_TIMER1 * 8
put64

push main
call

cli
hlt

//=====[ Printing ]=========================================

.const PRINTER_ADDR 0x00120

// Print a single character on the printer
// Arguments:
//   - return address
//   - character
print_char:
  swap
  push PRINTER_ADDR
  put8
  ret

// Print zero-terminated string on printer
// Arguments:
//   - return address
//   - pointer to start of the string
print_string:
  swap

 _ps_loop:
  dup
  push 1
  add
  swap
  get8 
  dup
  push print_char
  call
  push _ps_loop
  jmpi

  drop
  ret

//=====[ CRT ]==============================================

.const CRT_X 0x00130
.const CRT_Y 0x00132
.const CRT_SEND 0x00134
.const CRT_MOVE 0
.const CRT_LINE 1

// Moves the ray to given location
// Args:
//  - ret. addr
//  - 0 for move, != 0 for line
//  - y
//  - x
crt_move:
  // move the return address down
  push 2
  fswap
  // now [ret] [y] [type] [x]

  push CRT_X
  put16

  swap
  push CRT_Y
  put16

  push CRT_SEND
  put8

  ret

// Args:
//   ret
//   y
//   x
dup2:
  push 1
  pull
  // [x] [y] [ret] [y]
  push 3
  pull
  // [x] [y] [ret] [y] [x]
  push 1
  fswap
  // [x] [y] [x] [y] [ret]
  ret


//=====[ Timer interrupt ]==================================

// Value in RAM
.const XPOS 0x01000

handle_timer1:

  push XPOS
  get8
  dup
  dup
  push 240
  mul
  push 256 * 8
  add
  swap
  // [x] [x*256] [x]

  // Perform some hash on x
  dup
  mul
  push 6
  rsh
  push 0xff
  and

  // Also convert to screen space from 0-256 coordinates
  push 255
  swap
  sub
  push 240
  mul
  push 256 * 8
  add

  push 1
  push crt_move
  call

  // Increment X in memory
  push 1
  add
  push XPOS
  put8

  fini

//=====[ Main function ]====================================

msg:
  .ascii "Main function has awaken\n\0"

main:

  hlt
  push main
  jmp

  ret
