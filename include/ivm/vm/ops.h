/// \file
/// \brief Opcode implementations
///
/// Refer to instructions section in documentation
/// for what the mnemonics mean

#ifndef IVM_VM_OPS
#define IVM_VM_OPS

#include "ivm/vm/state.h"

// bool return value signifies if jump was performed
// If so, pc is not advanced after the operation

// Special instructions
bool vm_op_hlt(vm_state* state);

// Integer arithmetics
bool vm_op_add(vm_state* state);
bool vm_op_sub(vm_state* state);
bool vm_op_mul(vm_state* state);
bool vm_op_div(vm_state* state);
bool vm_op_mod(vm_state* state);
bool vm_op_sne(vm_state* state);
bool vm_op_rsh(vm_state* state);
bool vm_op_lsh(vm_state* state);
bool vm_op_or(vm_state* state);
bool vm_op_and(vm_state* state);
bool vm_op_xor(vm_state* state);

// Comparsion
bool vm_op_eq(vm_state* state);
bool vm_op_iszero(vm_state* state);
bool vm_op_lt(vm_state* state);
bool vm_op_le(vm_state* state);
bool vm_op_gt(vm_state* state);
bool vm_op_ge(vm_state* state);

// Stack manipulation
// (but it is'nt a stack anymore, since we are indexing it)
bool vm_op_drop(vm_state* state);
// push is done separately
bool vm_op_pull(vm_state* state);
bool vm_op_dup(vm_state* state);
bool vm_op_fswap(vm_state* state);
bool vm_op_swap(vm_state* state);

// Flow control
bool vm_op_jmp(vm_state* state);
bool vm_op_jmpi(vm_state* state);
bool vm_op_call(vm_state* state);
bool vm_op_int(vm_state* state);
bool vm_op_sti(vm_state* state);
bool vm_op_cli(vm_state* state);
bool vm_op_fini(vm_state* state);

// Memory
bool vm_op_get8(vm_state* state);
bool vm_op_get16(vm_state* state);
bool vm_op_get32(vm_state* state);
bool vm_op_get64(vm_state* state);

bool vm_op_put8(vm_state* state);
bool vm_op_put16(vm_state* state);
bool vm_op_put32(vm_state* state);
bool vm_op_put64(vm_state* state);

// (v2)
// Maybe that design was not the best for big instruction counts

// Second stack
bool vm_op_gsf(vm_state* state);
bool vm_op_ssf(vm_state* state);
bool vm_op_gsp(vm_state* state);
bool vm_op_ssp(vm_state* state);
bool vm_op_spush(vm_state* state);
bool vm_op_spop(vm_state* state);
bool vm_op_sfbegin(vm_state* state);
bool vm_op_sfend(vm_state* state);
bool vm_op_sfload8(vm_state* state);
bool vm_op_sfload16(vm_state* state);
bool vm_op_sfload32(vm_state* state);
bool vm_op_sfload64(vm_state* state);
bool vm_op_sfstore8(vm_state* state);
bool vm_op_sfstore16(vm_state* state);
bool vm_op_sfstore32(vm_state* state);
bool vm_op_sfstore64(vm_state* state);
bool vm_op_sadd(vm_state* state);

// Flow control
bool vm_op_rjmp(vm_state *state);
bool vm_op_rjmpi(vm_state *state);
bool vm_op_rcall(vm_state *state);
bool vm_op_gpc(vm_state *state);


#endif
