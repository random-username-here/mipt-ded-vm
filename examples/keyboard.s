.const INT_KB         0x08
.const KB_KEY_ADDR    0x150
.const KB_ACTION_ADDR 0x152
.const KB_MODS_ADDR   0x153

push handle_key
push INT_KB * 8
put64

// Go to the main function
push main
jmp

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
  jmp

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
  jmp


// Print the number on the stack
//
// == Inputs:
//   
//  - return address
//  - the number to print
//
// == Used addresses:
//
//  - [$01000 - $01016)
//

.const PN_BUF_ADDR 0x1000
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

//=====[ Interrupts ]==================================

kb_msg: .ascii "A key event happened: key \x1b[93m\0"

kb_msg_action:
  .i64 kb_msg_released
  .i64 kb_msg_pressed
  .i64 kb_msg_repeated

kb_msg_action_suf: .ascii "\x1b[0m \x1b[1m\0"

kb_msg_released: .ascii "was released\0"
kb_msg_pressed: .ascii "was pressed\0"
kb_msg_repeated: .ascii "repeated\0"

kb_msg_mods: .ascii "\x1b[0m with modifiers \x1b[93m\0"
kb_msg_suf: .ascii "\x1b[0m\n\0"

handle_key:

  push kb_msg
  push print_string
  call

  push KB_KEY_ADDR
  get16
  push print_number
  call

  push kb_msg_action_suf
  push print_string
  call

  push KB_ACTION_ADDR
  get8
  push 8
  mul
  push kb_msg_action
  add
  get64
  push print_string
  call

  push kb_msg_mods
  push print_string
  call

  push KB_MODS_ADDR
  get8
  push print_number
  call

  push kb_msg_suf
  push print_string
  call

  fini

//=====[ Main `function` ]==================================

msg:
  .ascii "Look, a number: \0"

// Main function
main:

  push msg
  push print_string
  call

  push -1234567890
  push print_number
  call

  push 10 // '\n'
  push print_char
  call


 _loop:
  hlt
  push _loop
  jmp

