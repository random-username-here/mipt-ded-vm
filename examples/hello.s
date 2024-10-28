// A "simple" program to write "Hello world" onto the screen
// FIXME: rewrite it to use printer, now we have a vector display

.const PRINTER_ADDR 0x00120

push main
call
push main
call

cli
hlt

// Here lives the constant section
message:
  .ascii "The machine says: Hello world!\n\0"

// Print character with printer 
// Args:
//   ret_addr - Return address
//   char - Character to write
putchar:
  swap
  push PRINTER_ADDR
  put8
  jmp
  

main:

  push message

 loop:
  // Read one byte of the message
  dup
  push 1
  add
  swap 
  get8 
  dup
  push PRINTER_ADDR
  put8 
  //push putchar
  //call
  push loop
  jmpi

  drop

  jmp


