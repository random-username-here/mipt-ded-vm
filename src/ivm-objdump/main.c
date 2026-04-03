/**
 * \brief A simple objdump for IVM
 * TODO: ilf format
 */
#include <endian.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ivm/common/util.h"
#include "ivm/common/macros.h"

const size_t LOAD_OFFSET = 0x10000; 
const size_t OP_PUSH = 0x21;

#define NO_PUSH INT64_MAX

const char *instrs[256] = {

  [0x00] = "hlt",

  // Arithmetics
  [0x01] = "add",
  [0x02] = "sub",
  [0x03] = "mul",
  [0x04] = "div",
  [0x05] = "mod",
  [0x06] = "sne",
  [0x07] = "lsh",
  [0x08] = "rsh",
  [0x09] = "or",
  [0x0a] = "and",
  [0x0b] = "xor",

  // Comparsion
  [0x10] = "eq",
  [0x11] = "iszero",
  [0x12] = "lt",
  [0x13] = "le",
  [0x14] = "gt",
  [0x15] = "ge",

  // Stack
  [0x20] = "drop",
  [0x21] = "push",
  [0x22] = "pull",
  [0x23] = "dup",
  [0x24] = "fswap",
  [0x25] = "swap",

  // Jumps & flow control
  [0x30] = "jmp",
  [0x31] = "jnz",
  [0x32] = "call",
  [0x33] = "int",
  [0x34] = "sti",
  [0x35] = "cli",
  [0x36] = "fini",

  // Memory
  [0x40] = "get8",
  [0x41] = "get16",
  [0x42] = "get32",
  [0x43] = "get64",
  [0x44] = "put8",
  [0x45] = "put16",
  [0x46] = "put32",
  [0x47] = "put64",

  // v2
  [0x50] = "gsf",
  [0x51] = "ssf",
  [0x52] = "gsp",
  [0x53] = "ssp",
  [0x54] = "spush",
  [0x55] = "spop",
  [0x56] = "sfbegin",
  [0x57] = "sfend",
  [0x58] = "sfload8",
  [0x59] = "sfload16",
  [0x5a] = "sfload32",
  [0x5b] = "sfload64",
  [0x5c] = "sfstore8",
  [0x5d] = "sfstore16",
  [0x5e] = "sfstore32",
  [0x5f] = "sfstore64",
  [0x60] = "sadd",

  [0x37] = "rjmp",
  [0x38] = "rjnz",
  [0x39] = "rcall",
  [0x3a] = "gpc",
};

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        puts("Expected one argument: a filename");
        return 1;
    }

    size_t size;
    unsigned char *data;
    if (!read_binary_file(argv[1], &size, (char**) &data)) {
        perror("Failed to read input file");
        return 1;
    }

    // NOTE: this assumes what we are getting valid push opcodes
    //       If we get incomplete push argument, we will read value outside of buffer.
    size_t pos = 0;
    int64_t pushVal = NO_PUSH;
    while (pos < size) {
        size_t opcode = data[pos];
        bool isPush = opcode == OP_PUSH;
        if (isPush) {
            memcpy(&pushVal, &data[pos+1], 8); // because that may be unaligned
            pushVal = le64toh(pushVal);
        }

        printf(ESC_GRAY "%8zx : " ESC_YELLOW "%02x ", LOAD_OFFSET + pos, data[pos]);
        if (isPush) {
            printf("%016lx ", pushVal);
        } else {
            printf("%16s ", "");
        }
        
        if (instrs[opcode] != NULL) {
            printf(ESC_BLUE "%-6s", instrs[opcode]);
            if (isPush) {
                printf(ESC_YELLOW " %ld", pushVal);
            }
        } else {
            printf(ESC_RED "(unknown instruction)");
        }

        if ((opcode == 0x30 || opcode == 0x31 || opcode == 0x31) && pushVal != NO_PUSH) {
            // jmp, jnz or call
            printf(" " ESC_YELLOW "0x%lx", pushVal);
        }

        if ((opcode == 0x37 || opcode == 0x38 || opcode == 0x39) && pushVal != NO_PUSH) {
            // jmp, jnz or call
            printf(" " ESC_YELLOW "0x%lx", pushVal + LOAD_OFFSET + pos);
        }
        printf(ESC_RST "\n");
        pos += 1 + (isPush ? 8 : 0);
        if (!isPush) pushVal = NO_PUSH;
    }

    free(data);
    return 0;
}
