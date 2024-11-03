#include "ivm/vm/mainloop.h"
#include "ivm/common/array.h"
#include "ivm/vm/mem.h"
#include "ivm/vm/ops.h"
#include "ivm/vm/state.h"
#include "ivm/vm/crt.h"
#include <stdint.h>
#include <stdio.h>

typedef bool (*op_fn)(vm_state* state);

const op_fn ops[256] = {
  // Special instructions
  [0x00] = vm_op_hlt,

  // Integer arithmetics
  [0x01] = vm_op_add,
  [0x02] = vm_op_sub,
  [0x03] = vm_op_mul,
  [0x04] = vm_op_div,
  [0x05] = vm_op_mod,
  [0x06] = vm_op_sne,
  [0x07] = vm_op_lsh,
  [0x08] = vm_op_rsh,
  [0x09] = vm_op_or,
  [0x0a] = vm_op_and,
  [0x0b] = vm_op_xor,

  // Comparsion
  [0x10] = vm_op_eq,
  [0x11] = vm_op_iszero,
  [0x12] = vm_op_lt,
  [0x13] = vm_op_le,
  [0x14] = vm_op_gt,
  [0x15] = vm_op_ge,

  // Stack manipulation
  [0x20] = vm_op_drop,
  // push is done separately
  [0x22] = vm_op_pull,
  [0x23] = vm_op_dup,
  [0x24] = vm_op_fswap,
  [0x25] = vm_op_swap,

  // Flow control
  [0x30] = vm_op_jmp,
  [0x31] = vm_op_jmpi,
  [0x32] = vm_op_call,
  [0x33] = vm_op_int,
  [0x34] = vm_op_sti,
  [0x35] = vm_op_cli,
  [0x36] = vm_op_fini,

  // Memory
  [0x40] = vm_op_get8,
  [0x41] = vm_op_get16,
  [0x42] = vm_op_get32,
  [0x43] = vm_op_get64,
  [0x44] = vm_op_put8,
  [0x45] = vm_op_put16,
  [0x46] = vm_op_put32,
  [0x47] = vm_op_put64

  // This is the day I learned how to use Vim macros
};

#define PUSH_OPCODE 0x21

void vm_mainloop(vm_state* state) {

  while (!state->should_die && !state->crt->was_closed) {

    while (state->num_waiting_to_ask_interrupts) {
      //state->log_fn(VM_LOG_INFO, "Waiting for interrupts to be written");
      pthread_mutex_lock(&state->interrupts_are_written_mutex);
      pthread_cond_wait(&state->interrupts_are_written, &state->interrupts_are_written_mutex);
      pthread_mutex_unlock(&state->interrupts_are_written_mutex);
    }

    pthread_mutex_lock(&state->mutex);

    vm_stack_val_t opcode;
    vm_mem_read(state, state->pc, 1, VM_MEM_EXEC, &opcode);
    check$(0 <= opcode && opcode <= 0xff, "One byte containing the opcode must be read");

    //printf("Executing opcode 0x%02x @ %p\n", opcode, (void*) (uintptr_t) state->pc);

    size_t advance = 1;
    if (opcode == PUSH_OPCODE) {
      vm_stack_val_t value;
      if (vm_mem_read(state, state->pc+1, 8, VM_MEM_EXEC, &value)) {
        ia_push$(&state->stack, value);
      }
      advance = 9;
    } else if (ops[opcode] == NULL) {
      vm_state_raise_exception(state, VM_EXC_UNKNOWN_OPCODE);
    } else {
      bool jumped = ops[opcode](state);
      advance = jumped ? 0 : 1;
    }

    state->pc += advance;

    size_t num_postponed = 0;

    for (size_t i = 0; i < ia_length(state->asked_interrupts); ++i) {
      if (!ia_length(state->interrupt_type) || 
          vm_interrupt_priority[ia_top$(state->interrupt_type)]
          < vm_interrupt_priority[state->asked_interrupts[i].type]) {

        // Interrupt happened, which interrupts us
        
        vm_stack_val_t handler = state->interrupt_vector[state->asked_interrupts[i].type];
        if (!handler && state->asked_interrupts[i].type == VM_INTR_EXCEPTION) {
          // No interrupt set for error handling interrupt
          if (state->log_fn) {
            vm_log_error(state); 
            state->log_fn(VM_LOG_ERROR, "No error handling interrupt was set, but error occured. Crashing.");
          }
          state->should_die = true;
          break;
        } else if (!handler) {
          // No interrupt handler for any other interrupt
          // Ignore this interrupt
          continue;
        }
        ia_push$(&state->interrupt_type, state->asked_interrupts[i].type);
        ia_push$(&state->interrupt_return_addr, state->pc);

        if (state->asked_interrupts[i].setup_state) {
          state->asked_interrupts[i].setup_state(
              state,
              state->asked_interrupts[i].data
          );
        }

        state->pc = handler;
      } else {
        if (num_postponed != i)
          state->asked_interrupts[num_postponed] = state->asked_interrupts[i];
        ++num_postponed;
      }
    }
    ia_resize$(&state->asked_interrupts, num_postponed);

    pthread_mutex_unlock(&state->mutex);
  }

  state->should_die = true;
  state->log_fn(VM_LOG_INFO, "Main loop exited");
}
