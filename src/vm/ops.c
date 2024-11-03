#include "ivm/vm/ops.h"
#include "ivm/common/array.h"
#include "ivm/vm/mem.h"
#include "ivm/vm/state.h"
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

/// \brief Extract top value from the stack
/// This raises an exception if stack underflows
static vm_stack_val_t get_val(vm_state* state, jmp_buf got_err) {
  
  check$(state, "State to pop values from expected");

  if (ia_length(state->stack) == 0) {
    vm_state_raise_exception(state, VM_EXC_STACK_UNDERFLOW);
    longjmp(got_err, 0);
  }

  vm_stack_val_t val = ia_top$(state->stack);
  ia_pop$(&state->stack);
  return val;
}

// The fun begins here

#define MAY_ERROR \
  jmp_buf jump_out;\
  if (setjmp(jump_out))\
    return false;

#define get$() get_val(state, jump_out)
#define cmd_fail$(why) do { vm_state_raise_exception(state, (why)); return false; } while(0)


//=====[ Special instructions ]=================================================


bool vm_op_hlt(vm_state* state) {
  vm_state_halt(state);
  return false;
}


//=====[ Integer arithmetics ]==================================================


bool vm_op_add(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() + get$());
  return false;
}

bool vm_op_sub(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t a = get$();
  ia_push$(&state->stack, get$() - a);
  return false;
}

bool vm_op_mul(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() * get$());
  return false;
}

bool vm_op_div(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t a = get$();
  if (a == 0)
    cmd_fail$(VM_EXC_DIVIDE_BY_ZERO);
  ia_push$(&state->stack, get$() / a);
  return false;
}

bool vm_op_mod(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t a = get$();
  if (a == 0)
    cmd_fail$(VM_EXC_DIVIDE_BY_ZERO);
  ia_push$(&state->stack, get$() % a);
  return false;
}

bool vm_op_sne(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t num_bytes = get$();
  vm_stack_val_t value = get$();

  if (num_bytes <= 0 || num_bytes > sizeof(vm_stack_val_t))
    cmd_fail$(VM_EXC_WRONG_SNE_SIZE);

  vm_stack_val_t ones = (1 << (sizeof(vm_stack_val_t) - num_bytes) * 8) - 1;

  value |= ones << num_bytes * 8;
  ia_push$(&state->stack, value);
  return false;
}

bool vm_op_rsh(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t dist = get$();
  ia_push$(&state->stack, get$() >> dist);
  return false;
}

bool vm_op_lsh(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t dist = get$();
  ia_push$(&state->stack, get$() << dist);
  return false;
}

bool vm_op_or(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() | get$());
  return false;
}

bool vm_op_and(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() & get$());
  return false;
}

bool vm_op_xor(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() ^ get$());
  return false;
}
//=====[ Comparsion ]===========================================================


bool vm_op_eq(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() == get$() ? 1 : 0);
  return false;
}

bool vm_op_iszero(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() == 0 ? 1 : 0);
  return false;
}

bool vm_op_lt(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() < get$() ? 1 : 0);
  return false;
}

bool vm_op_le(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() <= get$() ? 1 : 0);
  return false;
}

bool vm_op_gt(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() > get$() ? 1 : 0);
  return false;
}

bool vm_op_ge(vm_state* state) {
  MAY_ERROR
  ia_push$(&state->stack, get$() >= get$() ? 1 : 0);
  return false;
}


//=====[ Stack manipulation ]===================================================


bool vm_op_drop(vm_state* state) {
  MAY_ERROR
  get$();
  return false;
}

// push is done separately

bool vm_op_pull(vm_state* state) {
  MAY_ERROR

  vm_stack_val_t offset = get$();

  if (offset >= ia_length(state->stack))
    cmd_fail$(VM_EXC_STACK_UNDERFLOW);
  if (offset < 0)
    cmd_fail$(VM_EXC_BAD_TOP_OFFSET);

  ia_push$(&state->stack, state->stack[ia_length(state->stack) - 1 - offset]);
  return false;
}

bool vm_op_dup(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t v = get$();
  ia_push$(&state->stack, v);
  ia_push$(&state->stack, v);
  return false;
}

bool vm_op_fswap(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t offset = get$();
  vm_stack_val_t top = get$();

  if (offset >= ia_length(state->stack))
    cmd_fail$(VM_EXC_STACK_UNDERFLOW);
  if (offset < 0)
    cmd_fail$(VM_EXC_BAD_TOP_OFFSET);

  vm_stack_val_t t = state->stack[ia_length(state->stack) - 1 - offset];
  state->stack[ia_length(state->stack) - 1 - offset] = top;
  ia_push$(&state->stack, t);
  return false;
}

bool vm_op_swap(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t a = get$(), b = get$();
  ia_push$(&state->stack, a);
  ia_push$(&state->stack, b);
  return false;
}


//=====[ Flow control ]=========================================================


bool vm_op_jmp(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t addr = get$();
  state->pc = addr;
  return true;
}

bool vm_op_jmpi(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t addr = get$();
  vm_stack_val_t cond = get$();
  if (!cond)
    return false;
  state->pc = addr;
  return true;
}

bool vm_op_call(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t addr = get$();
  // This instruction as all is 1 byte long
  ia_push$(&state->stack, state->pc + 1);
  state->pc = addr;
  return true;
}

bool vm_op_int(vm_state* state) {
  MAY_ERROR
  vm_stack_val_t intr = get$();
  if (intr < 0 || intr >= NUM_INTERRUPTS)
    cmd_fail$(VM_EXC_BAD_INT_NUMBER);
  vm_state_trigger_interrupt(state, (vm_interrupt) {
      .type = intr,
      .data = NULL,
      .setup_state = NULL
  });
  return false;
}

bool vm_op_sti(vm_state* state) {
  state->interrupts_disabled = false;
  return false;
}

bool vm_op_cli(vm_state* state) {
  state->interrupts_disabled = true;
  return false;
}

bool vm_op_fini(vm_state* state) {

  if (!ia_length(state->interrupt_type))
    cmd_fail$(VM_EXC_FINI_NOT_IN_INTERRUPT);

  vm_stack_val_t ret_addr = ia_top$(state->interrupt_return_addr);
  vm_interrupt_type what = ia_top$(state->interrupt_type);

  ia_pop$(&state->interrupt_type);
  ia_pop$(&state->interrupt_return_addr);

  if (what == VM_INTR_EXCEPTION) {
    // Finished handling the exception,
    // reset exception values
    state->exception_pc = 0;
    state->exception_type = VM_EXC_NONE;
  }

  state->pc = ret_addr;
  return true;
}


//=====[ Memory ]===============================================================

#define VM_GENERIC_GET(size)\
  bool concat$(vm_op_get, size)(vm_state* state) { \
    MAY_ERROR\
    vm_stack_val_t data;\
    if (!vm_mem_read(state, get$(), size/8, VM_MEM_READ, &data))\
      return false;\
    ia_push$(&state->stack, data);\
    return false;\
  }

VM_GENERIC_GET(8)
VM_GENERIC_GET(16)
VM_GENERIC_GET(32)
VM_GENERIC_GET(64)

#undef VM_GENERIC_GET

#define VM_GENERIC_PUT(size)\
    bool concat$(vm_op_put, size)(vm_state* state) {\
      MAY_ERROR\
      vm_stack_val_t addr = get$();\
      vm_stack_val_t val = get$();\
      vm_mem_write(state, addr, size/8, val);\
      return false;\
    }

VM_GENERIC_PUT(8)
VM_GENERIC_PUT(16)
VM_GENERIC_PUT(32)
VM_GENERIC_PUT(64)

#undef VM_GENERIC_PUT

