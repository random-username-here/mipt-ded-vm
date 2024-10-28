# Assembler directives

## `.const <C name> <expression>` - Set a constant

Constants, like label names can be used in mathematical
expressions:

```asm
; Draw an `A` at (0,0) on the screen
.const VIDMEM_LOC 0x01000 ; Here we define a constant with given value
.const ASCII_A 65

push ASCII_A
push VIDMEM_LOC
put8
```

This directive defines a constant with given name and a given value.
If constant or label with such name already exists, you will get an error.

Note what constants are not evaluated in top to bottom order. They
can references later defined constants or labels:

```asm
.const bar baz * 4
.const baz 3
```

This will work without any problems.

## `.ascii <string>` - Emit a string

This directive emits bytes corresponding to given string literal.
No `\0` is added to the end - if you need it, write it yourself.

```asm
; Emit a string and compute its length
my_string:
    .ascii "Hello world!\n\0"
end_string:

.const STRING_LEN end_string - my_string
```

## `.base64 <string>` - Emit sequence of bytes encoded in base64

The main way to embed binary data into programs.

```asm
data:
    .base64 "SGVsbG8gd29ybGQhCg=="
end_data:
```

## `.[iu](8|16|32|64) <expression>` - Embed a number into program

This is fairly simple.

```asm
; Array of lengths of the months of the year
; A bunch of uint8 values here
month_lengths:
    ; Winter
    .u8 31
    .u8 28 ; Or 29, this example is too simple
    ; Spring
    .u8 31
    .u8 30
    .u8 31
    ; Summer
    .u8 30
    .u8 31
    .u8 31
    ; Autumn
    .u8 30
    .u8 31
    .u8 30
    ; Winter again
    .u8 31

; This function gets length of the month
; February is assumed to contain 28 days
;
; In: [month number, starting from 1]
; Out: [length of the month]
get_month_length:
    ; Convert to 0-indexed
    push 1
    sub
    ; Add to month_lengths
    push month_lengths
    add
    ; Get value
    get8

```

Everything is little-endian here.
