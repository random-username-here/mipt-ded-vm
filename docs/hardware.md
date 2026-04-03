# Memory map

This machine operates on 128K of memory.

They are mapped in this way:

Begin  | End    | Access | Usage
-------|--------|--------|----------
$00000 | $01000 | RW     | System IO
$01000 | $10000 | RWX    | 60K of free memory
$10000 | $20000 | RX     | ROM. All extra space is filled with zeroes.

# System IO map

## `$00000 - $00008` - Segfault place

Those bytes are reserved, but segfault each time you access them.

## `$00008` - `$00100` - Interrupt table

There are 31 interrupt slots, some of them are not used for now.

## `$00100` - `$00120` - Exception data

Here lives the data, which can be used to print detailed information
in case of an exception.

What is here:

 - `$00100` - 1 byte exception type, read `include/ivm/vm/state` for list of all exceptions.
 - `$00101-$00108` - 8 byte address where the exception happened
 - `$00109-$00111` - (for segfault) 8 byte address for which access failed

## `$00120` - `$00130` - Printer

You put characters in `$00120` to print them.

## `$00130` - `$00150` - CRT

 - `[$00130; $00132)` - X coordinate
 - `[$00132; $00134)` - Y coordinate
 - `$00134` - Put 0 here to move ray, 1 to draw a line

X and Y coordinates are unsigned, from 0 at top left corner to 65535 at bottom right.

## `$00150` - `$00160` - Keyboard

All the keyboard input is based on GLFW, so the keyboard interrupt (`$08`) is
just a wrapper around GLFW key callback.

 - `[$00150; $00152)` contains GLFW (v1) / Raylib (v2) key number of the pressed key.

 - `$00152` contains:
    - `0` if key was released
    - `1` if key was pressed
    - `2` if key was held down untill it repeated

 - `$00153` has the GLFW keyboard modifier bits, such as Control, Alt and Super.

Note what some of the complex keybindings may be impossible to enter because
they are mapped to something like closing the window. This program will not
capture all the input, unlike something serious like QEMU.
