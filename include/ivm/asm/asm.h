/// \brief The assembler itself
///

#ifndef IVM_ASM_ASM
#define IVM_ASM_ASM

#include "ivm/common/parser/span.h"
#include "ivm/asm/symtab.h"
#include "ivm/common/array.h"
#include <stddef.h>
#include <stdio.h>

/// A function to determine size of the given instruction
/// Note what this does not have access to the constant/label table
typedef size_t (*iasm_fn_to_get_instr_length)(ivm_span name, ia_arr$(ivm_span) args);

/// A function to emit bytecode of that instruction
typedef void (*iasm_fn_to_emit_instr)(ivm_span name, ia_arr$(ivm_span) args, ivm_symtab *symtab, FILE* out, size_t pc);

/// \brief A function to assemble given file
///
/// \param [out] output A file to write resulting bytecode to
/// \param source File with assembly to assemble
/// \param get_len Function which returns number of bytes used by that instruction
/// \param get_bytecode Function which emits bytecode of the given instruction
void iasm_assemble(FILE* output,
                  const char *source,
                  iasm_fn_to_get_instr_length get_len,
                  iasm_fn_to_emit_instr get_bytecode);

/// \brief Emit number as little-endian of given size and siggness
/// Usefull for all code in `arch/`
void iasm_emit_number(FILE* output, ivm_sym_value_t val, size_t num_bytes, bool no_sign, ivm_span ref);

#endif
