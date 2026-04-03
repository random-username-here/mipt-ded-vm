#include "ivm/asm/arch/ivm.h"
#include "ivm/asm/asm.h"
#include "ivm/asm/calc.h"
#include "ivm/asm/symtab.h"
#include "ivm/common/array.h"
#include "ivm/common/parser/error.h"
#include "ivm/common/parser/span.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <sysexits.h>
#include <stdlib.h>

enum ivm_flags {
    IF_NONE,
    IF_PUSH,
    IF_REL // rjmp, etc
};

typedef struct {

  char* mnemonic;
  uint8_t bytecode;
  enum ivm_flags flags;

} ivm_instruction_def;

#define OP_PUSH 0x21

const ivm_instruction_def instructions[] = {

  { "hlt", 0x00, IF_NONE },

  // Arithmetics
  { "add", 0x01, IF_NONE },
  { "sub", 0x02, IF_NONE },
  { "mul", 0x03, IF_NONE },
  { "div", 0x04, IF_NONE },
  { "mod", 0x05, IF_NONE },
  { "sne", 0x06, IF_NONE },
  { "lsh", 0x07, IF_NONE },
  { "rsh", 0x08, IF_NONE },
  { "or",  0x09, IF_NONE },
  { "and", 0x0a, IF_NONE },
  { "xor", 0x0b, IF_NONE },

  // Comparsion
  { "eq",     0x10, IF_NONE },
  { "iszero", 0x11, IF_NONE },
  { "lt",     0x12, IF_NONE },
  { "le",     0x13, IF_NONE },
  { "gt",     0x14, IF_NONE },
  { "ge",     0x15, IF_NONE },

  // Stack
  { "drop",     0x20, IF_NONE },
  { "push",     0x21, IF_PUSH },
  { "pull",     0x22, IF_NONE },
  { "dup",      0x23, IF_NONE },
  { "fswap",    0x24, IF_NONE },
  { "swap",     0x25, IF_NONE },

  // Jumps & flow control
  { "jmp",      0x30, IF_NONE },
  { "ret",      0x30, IF_NONE }, // An alias
  { "jmpi",     0x31, IF_NONE },
  { "jnz",      0x31, IF_NONE }, // (v2) alias

  { "call",     0x32, IF_NONE },
  { "int",      0x33, IF_NONE },
  { "sti",      0x34, IF_NONE },
  { "cli",      0x35, IF_NONE },
  { "fini",     0x36, IF_NONE },
  
  // Extra flow control (v2)
  { "rjmp",     0x37, IF_REL },
  { "rjnz",     0x38, IF_REL },
  { "rjmpi",    0x38, IF_REL }, // Alias for uniformity with older days
  { "rcall",    0x39, IF_REL },
  { "gpc",      0x3a, IF_NONE },

  // Memory
  { "get8",      0x40, IF_NONE },
  { "get16",     0x41, IF_NONE },
  { "get32",     0x42, IF_NONE },
  { "get64",     0x43, IF_NONE },
  { "put8",      0x44, IF_NONE },
  { "put16",     0x45, IF_NONE },
  { "put32",     0x46, IF_NONE },
  { "put64",     0x47, IF_NONE },

  // Second stack (v2)
  { "gsf",          0x50, IF_NONE },
  { "ssf",          0x51, IF_NONE },
  { "gsp",          0x52, IF_NONE },
  { "ssp",          0x53, IF_NONE },
  { "spush",        0x54, IF_NONE },
  { "spop",         0x55, IF_NONE },
  { "sfbegin",      0x56, IF_NONE },
  { "sfend",        0x57, IF_NONE },
  { "sfload8",      0x58, IF_NONE },
  { "sfload16",     0x59, IF_NONE },
  { "sfload32",     0x5a, IF_NONE },
  { "sfload64",     0x5b, IF_NONE },
  { "sfstore8",     0x5c, IF_NONE },
  { "sfstore16",    0x5d, IF_NONE },
  { "sfstore32",    0x5e, IF_NONE },
  { "sfstore64",    0x5f, IF_NONE },
  { "sadd",         0x60, IF_NONE },
};


static bool cmp_nocase(ivm_span name, const char* str) {

  for (size_t i = 0; i < ivm_span_get_length(name); ++i) {
    if (tolower(str[i]) != tolower(ivm_span_get_char(name, i)))
      return false;
  }
  return str[ivm_span_get_length(name)] == '\0';
}


static const ivm_instruction_def* lookup_instr(ivm_span name) {

  for (size_t i = 0; i < sizeof(instructions) / sizeof(instructions[0]); ++i) {
    if (cmp_nocase(name, instructions[i].mnemonic))
      return &instructions[i];
  }

  ivm_report(REPORT_ERROR, name, "Unknown instruction");
  exit(EX_DATAERR);
}


size_t iasm_ivm_inst_len(ivm_span name, ia_arr$(ivm_span) args) {

  const ivm_instruction_def* inst = lookup_instr(name);

  if (inst->flags == IF_PUSH && ia_length(args) == 0) {
    ivm_report(REPORT_ERROR, name,
               "`push` instruction expects something to push");
    exit(EX_DATAERR);
  }

  // some number of pushes + instruction (if it is not `push`)
  return (inst->flags == IF_PUSH ? 0 : 1) + ia_length(args) * 9;
} 

void iasm_ivm_emit_instr(ivm_span name, ia_arr$(ivm_span) args, ivm_symtab* symtab, FILE* out, size_t pc) {
  
  const ivm_instruction_def* inst = lookup_instr(name);

  for (size_t i = 0; i < ia_length(args); ++i) {
    ivm_sym_value_t param = ivm_compute_expression(
      name, "instruction parameter",
      args[i], symtab
    );
    if (i == ia_length(args) - 1 && inst->flags == IF_REL)
        param -= pc + 9 * ia_length(args) /* pushes */;
    fputc(OP_PUSH, out);
    iasm_emit_number(out, param, 8, false, args[i]);
  }

  if (inst->flags != IF_PUSH) {
      fputc(inst->bytecode, out);
  }
}


