#include "ivm/vm/state.h"
#include "ivm/common/array.h"
#include "ivm/common/macros.h"
#include "ivm/vm/mem.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define CHECKED(...) ? (die$(__VA_ARGS__),0) : 0


vm_state* vm_state_new (void) {

  vm_state* state = (vm_state*) calloc(1, sizeof(vm_state));
  check$(state, "VM state must be allocated for this to work");
 
  //---- Normal program stuff

  state->ram = (uint8_t*) calloc(RAM_SIZE, 1);
  check$(state->ram, "VM needs %d bytes of RAM", RAM_SIZE);

  // ROM is not mounted
  // Somebody will do that later
  state->rom = NULL;
  state->rom_size = 0;

  state->stack = ia_new_empty_array$(vm_stack_val_t) ;
  
  state->is_halted = false;

  //---- Interrupts

  state->interrupts_disabled = false;
  state->interrupt_type = ia_new_empty_array$(vm_interrupt_type);
  state->interrupt_return_addr = ia_new_empty_array$(vm_stack_val_t);
  state->asked_interrupts = ia_new_empty_array$(vm_interrupt);

  //---- Exceptions

  state->exception_pc = 0;
  state->exception_type = VM_EXC_NONE;

  //---- CRT
  // TODO: move this into constants CRT_SIZE
  state->crt_x = 0xffff / 2;
  state->crt_y = 0xffff / 2;

  //---- v2
  state->v2_sp = 0;
  state->v2_sf = 0;

  return state;
}

void vm_state_destroy(vm_state *state) {
  
  check$(state, "VM state to be destroyed must be present");

  free(state->ram);
}

void vm_state_trigger_interrupt(vm_state* state, vm_interrupt intr) {

  check$(state, "Should get a valid state");
  check$(intr.type < NUM_INTERRUPTS, "Interrupts are in range [0; %d), got number %d", NUM_INTERRUPTS, intr);

  if (!state->interrupts_disabled || intr.type == VM_INTR_EXCEPTION) {
    // Exception is non-maskable

    // Ask for interrupt
    ia_push$(&state->asked_interrupts, intr);

    if (state->is_halted) {
      // Machine is halted, it currently does nothing
      // Wake it up.
      state->is_halted = false;
    }
  }
}

void vm_state_halt(vm_state* state) {

  check$(state, "Should get a valid state");

  if (state->is_halted) {
    if (state->log_fn)
      state->log_fn(
          VM_LOG_WARNING,
          "Somehow you managed to halt while VM is already halted. This is 99.9% a bug."
      );
    return;
  }

  state->is_halted = true;
  if (state->interrupts_disabled) {
    if (state->log_fn)
      state->log_fn(
        VM_LOG_INFO,
        "The machine halted with interrupts disabled. It wont respond to anything anymore."
      );
    state->should_die = true;
    // Just return back to quit the loop
    return;
  }
}

void vm_state_mount_rom(vm_state* state, uint8_t* rom, size_t rom_size) {
  
  check$(state, "Should get a valid state");
  check$(rom_size == 0 || state, "ROM data pointer can only be NULL if ROM size is 0");

  state->rom = rom;
  state->rom_size = rom_size;
}

void vm_state_raise_exception(vm_state* state, vm_exception_type type) {

  check$(state, "Should get a valid state");
  check$(type != VM_EXC_NONE, "Should provide an exception to raise, but got no exception value");

  if (ia_length(state->interrupt_type) && ia_top$(state->interrupt_type) == VM_INTR_EXCEPTION) {
    // FIXME: print exception type
    if (state->log_fn) {
      vm_log_error(state); 
      state->log_fn(
          VM_LOG_ERROR,
          "Exception was raised inside an exception, so VM processor panicked."
      );
    }
    state->should_die = true;
  }

  // Exceptions are top priority,
  // so they cannot be postponed.
  // Also there cannot be two error interrupts requested
  // at the same time.
  state->exception_type = type;
  state->exception_pc = state->pc;

  vm_state_trigger_interrupt(state, (vm_interrupt) {
      .type = VM_INTR_EXCEPTION,
      .setup_state = NULL,
      .data = NULL
  });

}

const char* vm_strerr(vm_exception_type exc) {

#define VAR(i) case i: return #i;

  switch(exc) {
    VAR(VM_EXC_NONE)
    VAR(VM_EXC_DIVIDE_BY_ZERO)
    VAR(VM_EXC_STACK_UNDERFLOW)
    VAR(VM_EXC_WRONG_SNE_SIZE)
    VAR(VM_EXC_BAD_TOP_OFFSET)
    VAR(VM_EXC_BAD_INT_NUMBER)
    VAR(VM_EXC_FINI_NOT_IN_INTERRUPT)
    VAR(VM_EXC_SEGFAULT)
    VAR(VM_EXC_UNKNOWN_OPCODE)
    default:
      return "<unknown error>";
  }

#undef VAR
}

void vm_log_error(vm_state* state) {
  if (state->log_fn) {    
    state->log_fn(
        VM_LOG_ERROR,
        ESC_RED ESC_BOLD "%s " ESC_RST ESC_GRAY "@ " ESC_BLUE "pc " ESC_GRAY "= " ESC_YELLOW "%p" ESC_RST,
        vm_strerr(state->exception_type),
        state->exception_pc
    );

    if (state->exception_type == VM_EXC_SEGFAULT) {
      state->log_fn(
          VM_LOG_ERROR,
          "This happaned while accessing address " ESC_YELLOW "%p" ESC_RST,
          (void*) (uintptr_t) state->exception_segfault_addr
      );
    } else if (state->exception_type == VM_EXC_UNKNOWN_OPCODE) {
      vm_stack_val_t opcode;
      vm_mem_read(state, state->exception_pc, 1, VM_MEM_READ, &opcode);
      state->log_fn(
          VM_LOG_ERROR,
          "Opcode of this instruction is " ESC_YELLOW "%02x" ESC_RST, (int) opcode
      );
    }
    
    state->log_fn(
        VM_LOG_ERROR,
        "Regs: " ESC_BLUE "sp " ESC_GRAY "= " ESC_YELLOW "0x%05x"
        ESC_GRAY ", " ESC_BLUE "sf " ESC_GRAY "=" ESC_YELLOW " 0x%05x" ESC_RST,
        state->v2_sp, state->v2_sf
    );
    state->log_fn(
      VM_LOG_ERROR,
      "Stack contents: (from top down):"
    );

    const size_t MAX_ITEMS = 32;

    for (size_t i = 0; i < ia_length(state->stack) && i < MAX_ITEMS; ++i) {
      vm_stack_val_t val = state->stack[ia_length(state->stack) - 1 - i];
      state->log_fn(
        VM_LOG_ERROR,
        "    " ESC_BLUE "-%zu" ESC_GRAY " :  " ESC_YELLOW "%lld" ESC_GRAY " = " ESC_YELLOW "0x%x" ESC_RST, i, val, val
      );
    }

    if (ia_length(state->stack) > MAX_ITEMS) {
      state->log_fn(
        VM_LOG_ERROR,
        "  ... and %zu more items", ia_length(state->stack) - MAX_ITEMS
      );
    }

    if (ia_length(state->stack) == 0) {
      state->log_fn(
        VM_LOG_ERROR,
        ESC_GRAY "  <stack is empty>" ESC_RST
      );

    }
  }
}
