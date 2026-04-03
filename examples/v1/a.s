///
/// A program to print `A`
///

.const PRINTER_ADDR 0x00120

push 0x41
push PRINTER_ADDR
put8

cli
hlt

