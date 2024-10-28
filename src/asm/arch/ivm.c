#include "ivm/asm/arch/ivm.h"
#include "ivm/asm/asm.h"
#include "ivm/asm/calc.h"
#include "ivm/asm/symtab.h"
#include "ivm/common/parser/error.h"
#include "ivm/common/parser/span.h"
#include <ctype.h>
#include <stdint.h>
#include <sysexits.h>
#include <stdlib.h>

typedef struct {

  char* mnemonic;
  uint8_t bytecode;
  bool has_param;

} ivm_instruction_def;

const ivm_instruction_def instructions[] = {

  { "hlt", 0x00, false },

  // Arithmetics
  { "add", 0x01, false },
  { "sub", 0x02, false },
  { "mul", 0x03, false },
  { "div", 0x04, false },
  { "mod", 0x05, false },
  { "sne", 0x06, false },
  { "lsh", 0x07, false },
  { "rsh", 0x08, false },
  { "or",  0x09, false },
  { "and", 0x0a, false },
  { "xor", 0x0b, false },

  // Comparsion
  { "eq",     0x10, false },
  { "iszero", 0x11, false },
  { "lt",     0x12, false },
  { "le",     0x13, false },
  { "gt",     0x14, false },
  { "ge",     0x15, false },

  // Stack
  { "drop",     0x20, false },
  { "push",     0x21, true  },
  { "pull",     0x22, false },
  { "dup",      0x23, false },
  { "fswap",    0x24, false },
  { "swap",     0x25, false },

  // Jumps & flow control
  { "jmp",      0x30, false },
  { "ret",      0x30, false }, // An alias
  { "jmpi",     0x31, false },
  { "call",     0x32, false },
  { "int",      0x33, false },
  { "sti",      0x34, false },
  { "cli",      0x35, false },
  { "fini",     0x36, false },

  // Memory
  { "get8",      0x40, false },
  { "get16",     0x41, false },
  { "get32",     0x42, false },
  { "get64",     0x43, false },
  { "put8",      0x44, false },
  { "put16",     0x45, false },
  { "put32",     0x46, false },
  { "put64",     0x47, false },

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


size_t iasm_ivm_inst_len(ivm_span name, ivm_span args) {

  const ivm_instruction_def* inst = lookup_instr(name);

  if (inst->has_param && ivm_span_is_empty(args)) {
    ivm_report(REPORT_ERROR, name,
               "This instruction expects a parameter, but nothing was provided");
    exit(EX_DATAERR);
  }

  if (!inst->has_param && !ivm_span_is_empty(args)) {
    ivm_report(REPORT_ERROR, args, 
               "The instruction %s does not expect a parameter, but here it is", inst->mnemonic);
    exit(EX_DATAERR);
  }

  return 1 + (inst->has_param ? 8 : 0);
} 

void iasm_ivm_emit_instr(ivm_span name, ivm_span args, ivm_symtab* symtab, FILE* out) {
  
  const ivm_instruction_def* inst = lookup_instr(name);

  // Parameter was checked

  fputc(inst->bytecode, out);

  if (!inst->has_param)
    return;

  ivm_sym_value_t param = ivm_compute_expression(
      name, "instruction parameter",
      args, symtab
  );

  iasm_emit_number(out, param, 8, false, args);
}


