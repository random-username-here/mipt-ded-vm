.func
_start:
    rcall setup_stack
    rcall main

.func
setup_stack:
    push 0x10000
    dup
    ssp
    ssf
    ret

.include "printer.s"

.string
hello_world: .ascii "\nHello world!\n\0"

.func
main:
    rcall 0x40, print_char
    rcall hello_world, print_cstring
    rcall -1234567, print_number

    cli
    hlt

