<center>
    <img src="./assets/logo.svg">
</center>

A toy stack-based virtual machine with its own assembler.

Quick links:

 - [Assembly directives](./docs/asm-directives.md)
 - [Hardware description](./docs/hardware.md)
 - [Instruction list](./docs/instructions.md)
 - [Info about interrupts](./docs/interrupts.md)

## The features:

 - Machine
    - 4K of system memory, 60K of RAM for the program, unlimited amout of ROM
    - A "simple" way to print text into terminal
    - A vector fake CRT display
    - Interrupts, including interrupt `0x0f`, which is called
      when errors happen (for example, in case of division by zero).
    - Full set of instructions, not only the basic ones.
    - Can execute code from RAM

  - Assembler
    - Has constants, comments and other usefull stuff
    - Has rust-style (fancy) error reporting


If I add ability to change address when returning from interrupt, 
it would be possible to write an OS for this thing.

## TODO-s

 - A debugger, because triggering errors is not the best way
 - Context switching?
 - Fake disk I/O
 - A compiler, because writting assembly is hard.
