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

//=====[ Timer interrupt ]==================================

timed_msg:
  .ascii "This is executed by a timed interrupt!\n\0"

handle_timer1:
  push timed_msg
  push print_string
  call

  fini

//=====[ Main function ]====================================

msg:
  .ascii "Main function has awaken\n\0"

main:

  push msg
  push print_string
  call

  hlt
  push main
  jmp

  ret
