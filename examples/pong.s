/// 
/// A pong game
/// 
/// == Controls:
///
///  [q], [a] move the left paddle
///  [o], [l] move the right one
///

.const INT_TIMER1 0x01
.const INT_KB     0x08

// Setup interrupts

push handle_timer1
push INT_TIMER1 * 8
put64

push handle_key
push INT_KB * 8
put64

// Run the main function
push main
call

// Die
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

  // Print the number on the stack
//
// == Inputs:
//   
//  - return address
//  - the number to print
//
// == Used addresses:
//
//  - [$01800 - $01816)
//

.const PN_BUF_ADDR 0x1800
.const PN_BUF_SIZE 22

print_number:
  
  swap

  // Store ending \0
  push 0
  push PN_BUF_ADDR + PN_BUF_SIZE - 1
  put8

  push PN_BUF_ADDR + PN_BUF_SIZE - 2

 _pn_another_digit:
  // [num] [addr to put the digit]
  swap

  dup
  push 0
  le
  push _pn_not_negative
  jmpi 

  push 45 // '-'
  push print_char
  call

  push 0
  swap
  sub

_pn_not_negative:

  dup
  push 10
  mod
  push 48 // '0'
  add
  // [addr] [num] [digit]
  push 2
  pull
  // [addr] [num] [digit] [addr]
  put8
  // [addr] [num]
  push 10
  div
  swap
  push 1
  sub
  // [num] [next addr]
  push 1
  pull
  push _pn_another_digit
  jmpi

  push 1
  add

  push print_string
  call

  // drop [num]
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
  // [x] [y] [type] [ret]
  // move the return address down
  push 2
  fswap
  // now [ret] [y] [type] [x]

  push CRT_X
  put16

  // [ret] [y] [type]

  swap
  push CRT_Y
  put16

  push CRT_SEND
  put8

  ret


//=====[ Fixed-point values ]===============================

.const FP_SHIFT 16
.const FP_ONE 1 << FP_SHIFT
.const FP_ONEHALF FP_ONE >> 1




//=====[ Timer interrupt ]==================================

// 1/256-th of the screen per tick
.const SPEED_X 0xff * 3
.const SPEED_Y 0xff * 4 / 5
// 1/128-th of the screen in diameter
.const BALL_R FP_ONE >> 6
.const SCREEN_PAD FP_ONE / 32
.const PADDLE_SIZE FP_ONE / 4
.const PADDLE_ACCEL FP_ONE / 1000
.const PADDLE_MAXV FP_ONE / 100

// Value locations in RAM

// The ball 
.const XPOS   0x01000
.const YPOS   0x01008
.const VX     0x01010
.const VY     0x01018

// Pad positions, velocities and accelerations
.const LPAD   0x01020
.const LPAD_V 0x01028
.const LPAD_A 0x01030

.const RPAD   0x01038
.const RPAD_V 0x01040
.const RPAD_A 0x01048

.const DO_FRAME 0x01050
.const PAUSED   0x01051

msg_lost: .ascii "Somebody lost\n\0"

// Returns 1 or 0 if ball collides with the paddle
// Args:
//  - return address
//  - paddle location address (LPAD or RPAD)
collides_with_paddles:
  swap
  get64
  dup
  // [ret] [paddle Y]  [paddle Y]
  push YPOS
  get64
  ge

  swap
  // [ret] [is over top] [paddle Y]
  push PADDLE_SIZE
  add
  
  push YPOS
  get64
  le

  and
  push collision_bounced
  jmpi

  //push msg_lost
  //push print_string
  //call

  // No bounce, game was lost
  push reset
  call
  ret
  // Exit this interrupt
  //fini

 collision_bounced:
  push 0
  push VX
  get64
  sub
  push VX
  put64

  ret


// Draw the paddle
// Args:
//  - ret. addr
//  - paddle index (0 for left, 1 for right)
draw_paddle:
  swap

  dup
  dup
  push FP_ONE
  mul
  swap
  // [idx] [x coord] [idx]
  push LPAD
  swap
  push LPAD ^ RPAD
  mul
  // If idx = 1, then we do LPAD ^ LPAD ^ RPAD, else LPAD ^ 0.
  xor
  // Now we got LPAD or RPAD, corresponding to index.
  get64
  push space2screen
  call
  // Got Y position
  push 0
  push crt_move
  call

  // Same, but we add paddle size to Y
  dup
  push FP_ONE
  mul
  swap
  push LPAD
  swap
  push LPAD ^ RPAD
  mul
  xor
  get64
  push PADDLE_SIZE
  add
  push space2screen
  call
  push 1
  push crt_move
  call

  ret


// Convert [x] [y] into screen-space [x] [y]
// (this means adding some padding from screen edges to them)
space2screen:
  // [x] [y] [ret]
  push 1
  fswap
  // [ret] [y] [x]
  //push FP_ONE - SCREEN_PAD * 2
  //mul
  //push SCREEN_PAD
  //add

  swap
  // [ret] [x] [y]
  push FP_ONE - SCREEN_PAD * 2
  mul
  // required for fixed point multiplication to work
  push FP_SHIFT
  rsh
  push SCREEN_PAD
  add

  swap
  // [ret] [y] [x]
  push FP_ONE - SCREEN_PAD * 2
  mul
  push FP_SHIFT
  rsh
  push SCREEN_PAD
  add

  push 1
  fswap
  // [x] [y] [ret]
  ret

// Move the pad
// Args:
//  ret addr
//  pad data start addr 
mvpad:
  swap
  // [addr]

  dup
  push 8
  add
  get64 // got speed

  // [addr] [v]

  push 1
  pull
  push 16
  add
  get64 // got accel

  add

  dup
  push PADDLE_MAXV
  le
  push _mv_clamp_top
  jmpi 

  dup
  push -PADDLE_MAXV
  ge
  push _mv_clamp_bottom
  jmpi

  push _mv_clamped
  jmp

 _mv_clamp_top:
  drop
  push PADDLE_MAXV
  push _mv_clamped
  jmp

 _mv_clamp_bottom:
  drop
  push -PADDLE_MAXV
  push _mv_clamped
  jmp

 _mv_clamped:
  

  dup

  // [addr] [v'] [v']
  // store the speed
  push 2
  pull
  push 8
  add
  put64

  // [addr] [v']

  push 1
  pull
  get64 // get position

  add

  // [addr] [pos] [pos]

  dup
  push 0
  gt
  push _mv_top
  jmpi

  dup
  push FP_ONE - PADDLE_SIZE
  lt
  push _mv_bottom
  jmpi

  // [addr] [pos]

  swap
  put64
  push _mv_none
  jmp

 _mv_top: // collided with te top of the screen
  // [addr] [pos]
  drop
  // [addr]
  dup

  push 0
  swap
  put64

  // [addr]
  push 0
  swap
  // [0] [addr]
  push 8
  add
  put64

  push _mv_none
  jmp

 _mv_bottom:
  drop
  dup
  push FP_ONE - PADDLE_SIZE
  swap
  put64
  push 0
  swap
  push 8
  add
  put64
  push _mv_none
  jmp

 _mv_none:

  ret


// 100 times per sec
handle_timer1:

  push 1
  push DO_FRAME
  put8

  fini

frame:

  // Begin line from prev. ball position
  push XPOS
  get64
  push YPOS
  get64
  push space2screen
  call
  push 0
  push crt_move
  call

  push PAUSED
  get8
  push _f_draw
  jmpi

  //----- Update Y position

  // Get next Y coordinate
  push YPOS
  get64
  push VY
  get64
  add

  // Check if it is >= 1 or <= 0
  dup
  push FP_ONE
  le
  swap
  push 0
  ge
  or

  // If it is not, don't change direction
  iszero
  push dont_reverse_ydir
  jmpi

  // Y direction should be changed
  // *VY = 0 - *VY
  push 0
  push VY
  get64
  sub
  push VY
  put64

 dont_reverse_ydir:

  // Add Y velocity to Y position
  push YPOS
  get64
  push VY
  get64
  add
  push YPOS
  put64

  //----- Update X position

  // The thing is the same,
  // except we don't want to change directions all the times,
  // sometimes someone loses

  // Get next position
  push XPOS
  get64
  push VX
  get64
  add

  dup

  // [x] [x]

  // If it is >= 1 we collided with right border
  push FP_ONE
  le
  // [x] [cond]
  push collide_right
  jmpi

  // If it is <= 0 we collided with the left one
  push 0
  ge
  push collide_left
  jmpi

  // Otherwise, we didn't collide
  push collision_handled
  jmp

 collide_right:

  // Dispose of extra duplicate for second comparsion
  drop

  //push msg_right
  //push print_string
  //call
  push RPAD
  push collides_with_paddles
  call

  push collision_handled
  jmp

 collide_left:

  //push msg_left
  //push print_string
  //call
  push LPAD
  push collides_with_paddles
  call
  push collision_handled
  jmp

 collision_handled:

  //----- Move in X axis

  push XPOS
  get64
  push VX
  get64
  add
  push XPOS
  put64



  //----- Move the paddles

  push LPAD
  push mvpad
  call

  push RPAD
  push mvpad
  call

  //----- Draw everything

 _f_draw:

  push XPOS
  get64
  push YPOS
  get64
  push space2screen
  call

  push 1
  push crt_move
  call

  push 0
  push draw_paddle
  call
  push 1
  push draw_paddle
  call

  //push accel_msg
  //push print_string
  //call

  //push LPAD_A
  //get64
  //push print_number
  //call

  //push 10
  //push print_char
  //call

  ret

accel_msg: .ascii "Pad accel: \0"

.const KB_KEY_ADDR    0x150
.const KB_ACTION_ADDR 0x152
.const KB_MODS_ADDR   0x153

.const KEY_A 65
.const KEY_Q 81
.const KEY_O 79
.const KEY_L 76
.const KEY_SPACE 32


handle_key:
  
  push 0
  push DO_FRAME
  put8

  push KB_ACTION_ADDR
  get8

  dup
  // [action] [action]
  push 2 // key repeat, ignore it
  eq
  push _hk_fini
  jmpi

  // [action]

  push 2
  mul
  push 1
  sub

  // [+- 1]

  // Now we have -1 for release, 1 for press

  push KB_KEY_ADDR
  get8

  // [+-1] [keycode]

  push PAUSED
  get8
  push _hk_unpause_only
  jmpi

  dup
  // [+-1] [keycode]*2
  push KEY_Q
  eq
  push _hk_q
  jmpi 

  dup
  push KEY_A
  eq
  push _hk_a
  jmpi

  dup
  push KEY_O
  eq
  push _hk_o
  jmpi

  dup
  push KEY_L
  eq
  push _hk_l
  jmpi

 _hk_unpause_only:

  dup
  push KEY_SPACE
  eq
  push _hk_space
  jmpi
  // [+-1] [keycode]

  // No key matched
  drop // drop the keycode
  push _hk_fini
  jmp

 _hk_q:
  // [+-1] [keycode]
  drop // extra key scancode
  push LPAD_A
  get64
  // [+-1] [accel]
  swap // extract that 1/-1 we got at the start
  push PADDLE_ACCEL
  mul
  sub
  push LPAD_A
  put64
  fini
  
 _hk_a:
  drop
  push LPAD_A
  get64
  swap
  push PADDLE_ACCEL
  mul
  add
  push LPAD_A
  put64
  fini

 _hk_o:
  drop
  push RPAD_A
  get64
  swap
  push PADDLE_ACCEL
  mul
  sub
  push RPAD_A
  put64
  fini

 _hk_l:
  drop
  push RPAD_A
  get64
  swap
  push PADDLE_ACCEL
  mul
  add
  push RPAD_A
  put64
  fini

 _hk_space:
  drop
  push -1
  eq
  push _hk_no_unpause
  jmpi // unpause only on space press
  push 1
  push PAUSED
  get8
  sub
  push PAUSED
  put8
 _hk_no_unpause:
  fini

 _hk_fini:
  // [action] or [+-1]
  drop
  fini


reset:
  push FP_ONEHALF
  push XPOS
  put64
  push FP_ONEHALF
  push YPOS
  put64

  push SPEED_X
  push VX
  put64
  push SPEED_Y
  push VY
  put64

  push FP_ONEHALF - PADDLE_SIZE/2
  push LPAD
  put64

  push FP_ONEHALF - PADDLE_SIZE/2
  push RPAD
  put64

  push 0 
  push LPAD_V
  put64

  push 0 
  push LPAD_A
  put64

  push 0
  push RPAD_V
  put64

  push 0 
  push RPAD_A
  put64

  push 1
  push PAUSED
  put8

  ret

//=====[ Main function ]====================================

msg_left: .ascii "Left collision\n\0"
msg_right: .ascii "Right collision\n\0"


main:

  push reset
  call

 loop:

  hlt

  // something happaned
  push DO_FRAME
  get8
  iszero
  push _main_no_frame
  jmpi

  push frame
  call

 _main_no_frame:

  push loop
  jmp

  ret
