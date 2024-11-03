<img src="./assets/logo.svg" style="margin-bottom: 50px;">

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

From the point of the machine development:

 - A `.include` directive!
 - Linking
 - A debugger, because triggering errors is not the best way
 - Context switching?
 - Fake disk I/O
 - A compiler, because writting assembly is hard.

From the point of required tools/libraries/features
requiring a total rewrite to make this better:

 - Finish the documentation system
 - Make a threading library, because `pthread` lacks:
    - Good recursive mutexes  
      They may not exist on some machines, there is no way to exit
      all/return back (see `vm/state.c`, `vm_state_halt()`). 
    - Easy to use priority mutexes
    - `pthread`'s conditions are messy.
 - Allways return error codes, or something better. Never fail when the memory had ran out.
   (especially in `array`). This gives the possibility of `try`, `catch` and `catch unreachable` macro,
   like in Zig.
 - Add `..._assert_valid()` to everything.
 - Maybe add a stack trace in the error handling library.

