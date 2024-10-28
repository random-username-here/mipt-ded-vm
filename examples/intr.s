.const INT_EXCEPTION 0x0f
.const EXCEPTION_TYPE_ADDR 0x100
.const EXCEPTION_PC_ADDR   0x101
.const EXCEPTION_SF_MEM_ADDR   0x109

// Setup the exception handling interrupt
push handle_error
push INT_EXCEPTION * 8
put64

// Go to the main
push main
jmp

//=====[ Exception handling ]===============================

// Some data
error_msg:
  .ascii "\n\n!!!! Program crashed because of \0"
error_msg_2:
  .ascii ", which happaned at PC \0"
error_msg_end:
  .ascii "\n\n\n\0"
segfault_info:
  .ascii "\nAccess was performed to address \0"

err_none:            .ascii "<no exception type stored, you triggered the interrupt yourself>\0"
err_div_zero:        .ascii "division by zero\0"
err_stack_underflow: .ascii "stack underflow\0"
err_sne_size:        .ascii "bad number size for sign extension\0"
err_top_off:         .ascii "negative offset in pull/fswap instruction\0"
err_bad_int:         .ascii "bad interrupt index\0"
err_fini_not_in_int: .ascii "`fini` used not in interrupt\0"
err_segfault:        .ascii "segfault\0"
err_unknown_op:      .ascii "unknown opcode\0"


error_names:
  .i64 err_none
  .i64 err_div_zero
  .i64 err_stack_underflow
  .i64 err_sne_size
  .i64 err_top_off
  .i64 err_bad_int
  .i64 err_fini_not_in_int
  .i64 err_segfault
  .i64 err_unknown_op
  // more errors later


// The interrupt handler
handle_error:
  // Some code to print what happaned
  // We have return address on the stack, but it is not usefull here

  // Print error message
  push error_msg
  push print_string
  call

  // Load error name address and print it
  push EXCEPTION_TYPE_ADDR
  get8
  push 8
  mul
  push error_names
  add
  get64
  push print_string
  call

  // Another string
  push error_msg_2
  push print_string
  call

  // The address
  push EXCEPTION_PC_ADDR
  get64
  push print_address
  call

  // Extra segfault info (if it is segfault)
  push EXCEPTION_TYPE_ADDR
  get8
  push 7 // segfault
  eq
  iszero
  push _not_segfault
  jmpi 

  push segfault_info
  push print_string
  call

  push EXCEPTION_SF_MEM_ADDR
  get64
  push print_address
  call

 _not_segfault:

  // End string
  push error_msg_end
  push print_string
  call

  // Halt the system
  cli // Disable the interrupts
  hlt // Halt

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


// Print address on top of the stack
//   - return address
//   - the address
//
print_address:
  swap
  push 64
  // [addr] [index]
 _pa_loop:
  push 4
  sub
  dup
  // [addr] [index] [index]
  push 2
  pull
  // [addr] [index] [index] [addr]
  swap
  rsh
  push 0xf
  and

  // [addr] [index] [nibble]
  // Convert it to hex digit
  dup
  push 48
  swap 
  // ... [nibble] [48] [nibble]
  push 10
  le
  push 7
  mul
  add
  // ... [nibble] [48 or 65]
  add 

  push print_char
  call

  dup
  // [addr] [index] [index]
  push _pa_loop
  jmpi

  drop // remove [index]
  drop // remove [addr]
  
  jmp

//=====[ Main `function` ]==================================

msg:
  .ascii "doing some stuff...\n\0"

// Main function
main:

  push msg
  push print_string
  call

  // Cause an error
  push 0
  get8
  //push 0
  //div // Divide by zero

  cli
  hlt

