///
/// Relative call functionality && stack frames
///

.const PRINTER_ADDR 0x00120

begin:
    rcall setup_stack

    push 1, 2
    rcall add

    add 48 // '0'
    put8 PRINTER_ADDR 

    cli
    hlt

setup_stack:
    push 0x10000
    dup
    ssp
    ssf
    ret

add:
    spush
    sfbegin
    // ret addr is now at [sf + 8]
    add
    sfend
    spop
    ret
