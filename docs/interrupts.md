# Interrupts

There are 32 interrupts. Each interrupt has its entry in interrupt
table (`0x00000 - 0x00100`), which specifies what should be `call`-ed
when it happens.

To exit an interrupt do `fini`.

Example of exception handling interrupt:

```asm
.const INT_EXCEPTION 0x0f
.const EXCEPTION_TYPE_ADDR 0x100
.const EXCEPTION_PC_ADDR   0x101

// Setup the exception handling interrupt
push handle_error
push INT_EXCEPTION * 8
put64

// Go to the main
push main
jmp

// Some data
error_msg:
  .ascii "\n\nProgram crashed because of \0"
error_msg_2:
  .ascii " which happaned at PC \0"
error_msg_end:
  .ascii "\n"

err_div_zero: .ascii "division by zero\0"
err_stack_underflow: .ascii "stack underflow\0"

error_names:
  .i64 err_div_zero
  .i64 err_stack_underflow
  // ... more errors


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
  get8
  push print_address
  call

  // End string
  push error_msg_end
  push print_string
  call

  // Halt the system
  cli // Disable the interrupts
  hlt // Halt


// Main function
main:
  // Cause an error
  push 8
  push 0
  div // Divide by zero
```
