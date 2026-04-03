.func
.extern 
add:

.func
ivm_main:
    push 1
    push 2
    rcall add
    add 48
    put8 0x120 // print

