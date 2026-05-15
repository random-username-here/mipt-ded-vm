///
/// Bare-bones connection example
/// Will open a connection to example.com:80, send pre-recorded HTTP request
/// and print result.
///

.const INTR_MODEM 0x08

// Buffer for data
.const BUF_BEGIN 0x1000 // ram begin
.const BUF_SIZE 0x1000 // 4K

// Modem addresses
.const MDM_A      0x160
.const MDM_B      0x168
.const MDM_STAT   0x170
.const MDM_CMD    0x171
.const MDM_ERR    0x172

// Status codes
.const MDM_S_LINE 0x01
.const MDM_S_CONN 0x02
.const MDM_S_INCM 0x04
.const MDM_S_FAIL 0x08

// Commands
.const MDM_C_CONN 0x01
.const MDM_C_QUIT 0x02
.const MDM_C_SEND 0x03
.const MDM_C_RECV 0x04

// Errors
.const MDM_E_NONE      0x00
.const MDM_E_BADCMD    0x01
.const MDM_E_NOLINE    0x02
.const MDM_E_CONNFAIL  0x03
.const MDM_E_CONNRESET 0x04
.const MDM_E_SEGFAULT  0x05

///
/// Main function
///
.func
main:
    // Setup second stack
    rcall setup_stack

    // Setup data recived interrupt
    put64 int_modem, 0x08 + INTR_MODEM  * 0x08

    // Connect
    rcall modem_connect
    rjnz _conn_succesfull
    rcall msg_conn_err, print_cstring
    rcall quit
_conn_succesfull:
    rcall msg_conn, print_cstring

    // Send some data
    rcall data, data_len, modem_send
    rjnz _send_succesfull
    rcall msg_send_err, print_cstring
    rcall quit
_send_succesfull:
    rcall msg_send, print_cstring

    // Spin infinite loop, handling interrupts
_loop:
    hlt
    get8 MDM_STAT
    and MDM_S_FAIL
    rjnz _fail
    rjmp _loop

_fail:
    get8 MDM_ERR
    eq MDM_E_CONNRESET
    rjmp _reset

    rcall msg_fail, print_cstring
    rcall quit

_reset:
    rcall msg_reset, print_cstring
    rcall quit

///
/// Modem interrupt handler
///
.func
int_modem:
    call msg_int_mdm, print_cstring

    // recv
    call BUF_BEGIN, BUF_SIZE, modem_recv
    dup // for length & setting \0

    // print length
    call msg_recv_size_begin, print_cstring
    call print_number
    call msg_recv_size_end, print_cstring

    // terminate string with \0
    add BUF_BEGIN
    push 0
    swap
    put8

    // print data
    call BUF_BEGIN, print_cstring

    fini

//======================================
// Modem


///
/// Connect to server
/// Args: none
/// Returns: [is succesfull? 0/1]
///
.func
modem_connect:
    // set params
    put64 host, MDM_A
    put64 PORT, MDM_B
    // connect (in vm will block program for now)
    put8 MDM_C_CONN, MDM_CMD
    // get status, check if failed
    get8 MDM_STAT
    and MDM_S_FAIL
    iszero
    // ret addr <-> ret value
    swap 
    ret

///
/// Send buffer to server
/// Args: [buffer] [size]
/// Returns: [is succesfull? 0/1]
///
.func
modem_send:
    // stack: [buf] [size] [ret addr]
    // size -> B
    swap
    put64 MDM_B
    // addr -> A
    swap
    put64 MDM_A
    put8 MDM_C_SEND, MDM_CMD
    // check status, return
    get8 MDM_STAT
    and MDM_S_FAIL
    iszero
    // ret addr <-> ret value
    swap 
    ret

///
/// Recive data from server
/// Args: [buffer] [max size]
/// Returns [count (0 if error)]
///
.func
modem_recv:
    // stack: [buf] [size] [ret addr]
    swap
    put64 MDM_B
    swap
    put64 MDM_A
    put8 MDM_C_RECV, MDM_CMD
    get64 MDM_B
    swap
    ret

///
/// Setup second stack
///
.func
setup_stack:
    push 0x10000
    dup
    ssp
    ssf
    ret

.func
quit:
    cli
    hlt

// Printing functions
.include "printer.s"

/// Connection data
.const          PORT 80
host:           .ascii "example.com\0"
//.const          PORT 3000
//host:           .ascii "localhost\0"


/// Request
data:           .ascii "GET / HTTP/1.1\r\n"
                .ascii "Host: example.com\r\n"
                .ascii "User-Agent: ivm-modem\r\n"
                .ascii "Accept: */*\r\n"
                .ascii "Connection: close\r\n"
                .ascii "\r\n\0"

data_end: // TODO: implement `.` in computations
.const data_len data_end - data

msg_conn_err:           .ascii "Failed to connect\n\0"
msg_conn:               .ascii "Connected\n\0"
msg_send_err:           .ascii "Failed to send\n\0"
msg_send:               .ascii "Sent a message, now waiting for response\n\0"
msg_int_mdm:            .ascii "Modem says we have some data to read\n\0"
msg_recv_size_begin:    .ascii "Recived \0"
msg_recv_size_end:      .ascii " bytes:\n\0"
msg_fail:               .ascii "Modem reports failiure\n\0"
msg_reset:              .ascii "Connection closed by foreign host\n\0"
