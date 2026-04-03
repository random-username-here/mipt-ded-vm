///=========================================================
/// printer.s -- Reuseable printing library for IVM
///=========================================================

///
/// Print a single character
///
/// In: [charcode] [ret]
/// Out: none
///
.func
print_char:
    swap
    put8 0x00120
    ret

///
/// Printe zero-terminated string
///
/// In: [address] [ret]
/// Out: 
///
.func
print_cstring:
    swap
_print_cstring_loop:
    dup
    get8
    dup
    iszero
    jnz _print_cstring_end
    rcall print_char
    add 1
    jmp _print_cstring_loop
_print_cstring_end:
    drop
    drop
    ret

///
/// Print top number on the stack
///
/// In: [num] [ret]
/// Out: none
///
/// Implemented as:
///  - Number negative -> print `-`, negate it to being positive
///  - Build string to print on stack, as C-string.
///  - Print resulting c-string.
///
.func
print_number:
    spush
    sfbegin

    // if negative -> print `-` ane negate
    dup
    lt 0
    jnz _print_number_positive
    rcall 0x2d, print_char // `-`
    push 0
    swap
    sub


_print_number_positive:
    sfstore8 0, -1
    push -2
    // [num] [stack offset]

_print_number_digit:
    pull 1
    mod 10
    add 0x30 // `0`
    pull 1
    sfstore8
    sub 1
    swap
    div 10
    dup
    iszero
    jnz _print_number_end
    swap
    jmp _print_number_digit

_print_number_end:
    drop
    gsf
    add
    add 1
    rcall print_cstring
    sfend
    spop
    ret

