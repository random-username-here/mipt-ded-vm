# Instruction set

TODO: add alternate push opcodes to push values smaller then 8 bytes.

All opcodes here are hexamedical.
If any errors happen (division by zero, etc), interrupt `0x0f` is triggered.

## Special instructions

 - `00 hlt` Halt the processor.

## Integer arithmetics

 - `01 add` Add two top items on stack
 - `02 sub` Substract top item on the stack from second
 - `03 mul` Multiply two top items
 - `04 div` Divide second item by the top one
 - `05 mod` Get modulo remainder of dividing second item by the top item
 - `06 sne` Sign extend second item on the stack from N byte integer to full stack item size.
 - `07 lsh` Left shift 
 - `08 rsh` Right shift 

## Comparsion

 - `10 eq` Check if top two values are equal. This does not care what type they are.
 - `11 iszero` Check if top value has all bits zero. If yes, put 1 on stack, else 0

 - `12 lt` Return if top integer is less than second
 - `13 le`
 - `14 gt`
 - `15 ge`

## Basic stack stuff

 - `20 drop` Drop top item of the stack for some reason
 - `21 push <value>` Push a fixed value onto stack
 - `22 pull` Read value at given offset from the stack.
             That value does not count, so offset `0` means value before the top one.
 - `23 dup`  Duplicate the top item on the stack.
 - `24 fswap` Same as pull, except it swaps the value with the top one.
              0 is the value under the two top ones (the one to swap with and distance).
 - `25 swap` Swap

## Jumps & flow control

`ret`, which is used in most assemblers here is the same as `jmp`.

 - `30 jmp` Jump to the address at the top of the stack
 - `31 jmpi` Jump if the second item is nonzero
 - `32 call` Jump, saving address after this instruction at the top of the stack
 - `33 int` Trigger an interrupt with given number
 - `34 sti` Enable interrupts
 - `35 cli` Disable interrupts
 - `36 fini` Finish handling the interrupt

## Memory

 - `40 get8` Read a byte from the memory at address at specified location
 - `41 get16` Read a short value
 - `42 get32`
 - `43 get64`
 - `44 put8` Put second item-s last 8 bits at address specified by the top item
 - `45 put16`
 - `46 put32`
 - `47 put64`

