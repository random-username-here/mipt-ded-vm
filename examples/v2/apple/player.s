///
/// Vectorized video player
///

.include "video.gen.s"

.func
main:
    rcall setup_stack
    put64 int_timer1, 0x8
_main_loop:
    hlt
    rjmp _main_loop

.func
setup_stack:
    push 0x10000
    dup
    ssp
    ssf
    ret

///=========================================================
/// CRT routines
///=========================================================

.const CRT_ADDR_X   0x00130
.const CRT_ADDR_Y   0x00132
.const CRT_ADDR_CMD 0x00134

///
/// Paint on CRT.
/// Moves ray from current position to (X, Y),
/// drawing line/not drawing line based on ENABLE.
///
/// In: [x] [y] [enable] [ret]
/// Out:  none
///
.func
crt:
    fswap 2 // [x] <-> [ret]
    put16 CRT_ADDR_X
    swap // [enable] <-> [y]
    put16 CRT_ADDR_Y
    put8 CRT_ADDR_CMD
    ret

///=========================================================
/// Font interpretation
///=========================================================
/// Letter program consists of some number of commands.
/// Each command is 64-bit wide.
/// First 8 bits means operation: 1 for move, 2 for draw, 0 end.
/// other unused for now. Last 16 and 16 are absolute X/Y.
/// Letters are assumed to be 2^16 x 2^16.
/// 
.const FNT_C_END  0
.const FNT_C_MOVE 1
.const FNT_C_DRAW 2

///
/// Draws one letter of the font
///
/// In: [letter] [ws] [hs] [x] [y] [ret]
///
.func
fnt_letter:
    spush
    sfbegin
    spush // y      @ -8
    spush // x      @ -16
    spush // hs     @ -24
    spush // ws     @ -32
    spush // letter @ -40

    sfload64 -40
    mul 8
    add outlines_frametable
    get64

_fnt_letter_loop:
    // [cmd addr]
    dup
    get64
    // [addr] [cmd]

    // get operation
    dup
    and 0xff

    // check if it is letter end? (=0)
    dup
    iszero
    rjnz _fnt_letter_end

    // else convert it to crt enable: 1 -> 0 (move), 2 -> 1 (draw)
    sub 1
    // [addr] [cmd] [mode]
    swap
    // get y
    dup
    rsh 64 - 32
    and 0xffff
    sfload64 -24
    div
    sfload64 -8
    add
    // [addr] [mode] [cmd] [y]
    // now x
    swap
    rsh 64 - 16
    and 0xffff
    sfload64 -32
    div
    sfload64 -16
    add
    // [addr] [mode] [y] [x]
    fswap 1
    rcall crt
    // advance addr
    add 8
    rjmp _fnt_letter_loop

_fnt_letter_end:
    drop
    drop
    drop
    sfend
    spop
    ret

///=========================================================
/// Program
///=========================================================
   
.const ADDR_COUNTER 0x1000

///
/// Timer1 interrupt, runs each second
///
.func
int_timer1:
    get64 ADDR_COUNTER
    rcall 1, 1, 0, 8000, fnt_letter
    get64 ADDR_COUNTER
    add 1
    mod outlines_framecount
    put64 ADDR_COUNTER
    fini
