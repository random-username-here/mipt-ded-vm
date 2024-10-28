#include "ivm/vm/state.h"
#include "ivm/common/array.h"
#include "ivm/common/macros.h"
#include "ivm/vm/mem.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

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
  state->interrupt_type = ia_new_empty_array$(vm_interrupt);
  state->interrupt_return_addr = ia_new_empty_array$(vm_stack_val_t);
  state->asked_interrupts = ia_new_empty_array$(vm_stack_val_t);

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr) CHECKED("Must init mutex attr");
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) CHECKED("Must set mutex attr type");

  pthread_mutex_init(&state->mutex, &attr) CHECKED("Mutex must be initialized");
  pthread_mutex_init(&state->wake_up_mutex, &attr) CHECKED("Mutex must be initialized");
  pthread_mutex_init(&state->interrupts_are_written_mutex, &attr) CHECKED("Mutex must be initialized");

  pthread_mutexattr_destroy(&attr) CHECKED("Must destroy the mutex attr");

  pthread_cond_init(&state->wake_up, NULL) CHECKED("Wakeup condition must initialize");
  pthread_cond_init(&state->interrupts_are_written, NULL) CHECKED("Condition must initialize");

  //---- Exceptions

  state->exception_pc = 0;
  state->exception_type = VM_EXC_NONE;

  return state;
}

void vm_state_destroy(vm_state *state) {
  
  check$(state, "VM state to be destroyed must be present");

  free(state->ram);

  pthread_cond_destroy(&state->wake_up);
  pthread_cond_destroy(&state->interrupts_are_written);

  pthread_mutex_destroy(&state->mutex);
  pthread_mutex_destroy(&state->wake_up_mutex);
  pthread_mutex_destroy(&state->interrupts_are_written_mutex);
}

void vm_state_trigger_interrupt(vm_state* state, vm_interrupt intr) {

  check$(state, "Should get a valid state");
  check$(intr < NUM_INTERRUPTS, "Interrupts are in range [0; %d), got number %d", NUM_INTERRUPTS, intr);

  state->log_fn(VM_LOG_INFO, "Triggering an interrupt");

  // TODO: better understand this hell
  // https://stackoverflow.com/questions/11666610/how-to-give-priority-to-privileged-thread-in-mutex-locking
  ++state->num_waiting_to_ask_interrupts;
  pthread_mutex_lock(&state->mutex);
  --state->num_waiting_to_ask_interrupts;
  
  state->log_fn(VM_LOG_INFO, ".. locked the state");

  if (!state->interrupts_disabled) {

    // Ask for interrupt
    ia_push$(&state->asked_interrupts, intr);

    if (state->is_halted) {
      // Machine is halted, it currently does nothing
      // Wake it up.
      state->log_fn(VM_LOG_INFO, ".. waking up the machine");
      pthread_mutex_lock(&state->wake_up_mutex);
      pthread_cond_signal(&state->wake_up);
      pthread_mutex_unlock(&state->wake_up_mutex);
    }
  }

  pthread_mutex_lock(&state->interrupts_are_written_mutex);
  pthread_cond_signal(&state->interrupts_are_written);
  pthread_mutex_unlock(&state->interrupts_are_written_mutex);

  pthread_mutex_unlock(&state->mutex);
}

void vm_state_halt(vm_state* state) {

  check$(state, "Should get a valid state");

  // No asking for interrupts while we are doing stuff
  pthread_mutex_lock(&state->mutex);

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
    pthread_mutex_unlock(&state->mutex);
    return;
  }

  pthread_mutex_unlock(&state->mutex);
  pthread_mutex_unlock(&state->mutex); // FIXME! we unlock mutex locked second time by mainloop

  state->log_fn(VM_LOG_INFO, "Halted, waiting wake up call");
  pthread_mutex_lock(&state->wake_up_mutex);
  state->log_fn(VM_LOG_INFO, ".. locked wake up mutex");

  bool should_wake_up = false;
  while (!should_wake_up) {

    // Wait until someone calls for wakeup
    state->log_fn(VM_LOG_INFO, ".. waiting for condition");
    pthread_cond_wait(&state->wake_up, &state->wake_up_mutex);
    state->log_fn(VM_LOG_INFO, ".. somebody triggered a wake up call");

    // Check if they have something worth waking up for
    pthread_mutex_lock(&state->mutex);
    state->log_fn(VM_LOG_INFO, ".. %zu interrupts asked", ia_length(state->asked_interrupts));

    for (size_t i = 0; i < ia_length(state->asked_interrupts); ++i) {
      if (!ia_length(state->interrupt_type) || 
          vm_interrupt_priority[ia_top$(state->interrupt_type)]
          < vm_interrupt_priority[state->asked_interrupts[i]]) {
        // This is more important then what we are doing now
        should_wake_up = true;
        state->is_halted = false;
        break;
      }
    }

    pthread_mutex_unlock(&state->mutex);
  }

  pthread_mutex_unlock(&state->wake_up_mutex);
  pthread_mutex_lock(&state->mutex); // FIXME: we lock back locked mutex by mainloop
}

void vm_state_mount_rom(vm_state* state, uint8_t* rom, size_t rom_size) {
  
  check$(state, "Should get a valid state");
  check$(rom_size == 0 || state, "ROM data pointer can only be NULL if ROM size is 0");

  pthread_mutex_lock(&state->mutex);

  state->rom = rom;
  state->rom_size = rom_size;

  pthread_mutex_unlock(&state->mutex);
}

void vm_state_raise_exception(vm_state* state, vm_exception_type type) {

  check$(state, "Should get a valid state");
  check$(type != VM_EXC_NONE, "Should provide an exception to raise, but got no exception value");

  pthread_mutex_lock(&state->mutex);

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

  state->exception_type = type;
  state->exception_pc = state->pc;

  vm_state_trigger_interrupt(state, VM_INTR_EXCEPTION);

  pthread_mutex_unlock(&state->mutex);
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
        "%s  @ pc = %p",
        vm_strerr(state->exception_type),
        state->exception_pc
    );

    if (state->exception_type == VM_EXC_SEGFAULT) {
      state->log_fn(
          VM_LOG_INFO,
          "This happaned while accessing address %p",
          (void*) (uintptr_t) state->exception_segfault_addr
      );
    } else if (state->exception_type == VM_EXC_UNKNOWN_OPCODE) {
      vm_stack_val_t opcode;
      vm_mem_read(state, state->exception_pc, 1, VM_MEM_READ, &opcode);
      state->log_fn(
          VM_LOG_INFO,
          "Opcode of this instruction is %02x", (int) opcode
      );
    }

    state->log_fn(
      VM_LOG_INFO,
      "Stack contents: (from top down):"
    );

    const size_t MAX_ITEMS = 32;

    for (size_t i = 0; i < ia_length(state->stack) && i < MAX_ITEMS; ++i) {
      state->log_fn(
        VM_LOG_INFO,
        "  top - %zu:  %lld", i, state->stack[ia_length(state->stack) - 1 - i]
      );
    }

    if (ia_length(state->stack) > MAX_ITEMS) {
      state->log_fn(
        VM_LOG_INFO,
        "  ... and %zu more items", ia_length(state->stack) - MAX_ITEMS
      );
    }

    if (ia_length(state->stack) == 0) {
      state->log_fn(
        VM_LOG_INFO,
        "  <stack is empty>" 
      );

    }
  }
}
