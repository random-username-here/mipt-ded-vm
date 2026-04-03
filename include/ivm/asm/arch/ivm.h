/// \file
/// \brief Instruction emmitting for IVM

#ifndef IVM_ASM_ARCH_IVM
#define IVM_ASM_ARCH_IVM

#include "ivm/common/parser/span.h"
#include "ivm/asm/symtab.h"

size_t iasm_ivm_inst_len(ivm_span name, ia_arr$(ivm_span) args);

void iasm_ivm_emit_instr(ivm_span name, ia_arr$(ivm_span) args, ivm_symtab* symtab, FILE* out, size_t pc);

#endif
