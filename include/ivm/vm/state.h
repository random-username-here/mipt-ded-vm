/// \file
/// \brief State of the virtual machine
///
/// Lots of multithreading here, because of interrupts
/// (this is needed for IO and timer interrupts)
///

#ifndef IVM_VM_STATE
#define IVM_VM_STATE

#include "ivm/common/array.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define NUM_INTERRUPTS 31

#define SYSMEM_END   0x01000
#define RAM_END      0x10000
#define ROM_END      0x20000

#define RAM_SIZE     (RAM_END - SYSMEM_END)
#define ROM_MAX_SIZE (ROM_END - RAM_END)

typedef enum {

  VM_INTR_TIMER1 = 0x01,

  // Key callback
  VM_INTR_KEY = 0x08,

  /// Interrupt which happens when something bad happened
  /// (like stack underflow)
  VM_INTR_EXCEPTION = 0x0f,

  VM_INTR_NONE = NUM_INTERRUPTS

} vm_interrupt_type;

struct vm_state;

typedef struct {
  vm_interrupt_type type;
  void* data;
  void (*setup_state)(struct vm_state* machine, void* data);
} vm_interrupt;

static const size_t vm_interrupt_priority[NUM_INTERRUPTS] = {

  [VM_INTR_TIMER1] = 1,

  [VM_INTR_KEY] = 2,

  /// \brief An exception happened (like division by zero)
  /// **Nothing can interrupt this!** This thing MUST
  /// have the highest priority.
  [VM_INTR_EXCEPTION] = 32 

};


typedef enum {
  VM_EXC_NONE = 0,

  // Used by: div, mod
  //
  // We divided by zero
  VM_EXC_DIVIDE_BY_ZERO  = 1,

  // Used by: Any instruction which reads the stack
  //
  // We did not provide enough elements on the stack
  VM_EXC_STACK_UNDERFLOW = 2,

  // Used by: sne
  //
  // Got N byte integer, where N is specified to be <= 0 or > sizeof(stack elemen)
  VM_EXC_WRONG_SNE_SIZE = 3,

  // Used by: pull, fswap
  //
  // Expected offset >= 0 from the top of the stack, but
  // got a negative number
  VM_EXC_BAD_TOP_OFFSET = 4,

  // Used by: int
  //
  // `int` got nonexistent interrupt index
  VM_EXC_BAD_INT_NUMBER = 5,

  // Used by: fini
  //
  // `fini` called not inside an interrupt
  VM_EXC_FINI_NOT_IN_INTERRUPT = 6,

  // Used by: memory access functions
  //
  // Raised when you write to non-writable locations,
  // read nonexistent memory or try to execute data in
  // system control segment.
  //
  VM_EXC_SEGFAULT = 7,

  // Used by: the processor when unknown opcode is found
  VM_EXC_UNKNOWN_OPCODE = 8,

  _VM_NUM_EXC
} vm_exception_type;

/// Types of logs emitted by the VM
/// using the `log_fn` of `vm_state`.
typedef enum {
  VM_LOG_INFO,
  VM_LOG_WARNING,
  VM_LOG_ERROR
} vm_log_type;


typedef int64_t vm_stack_val_t;

struct vm_crt;

typedef struct vm_state {

  //---- Normal program stuff

  // The program counter
  vm_stack_val_t pc;

  // 60K of memory mounted at 0x01000
  // Readable, writeable, executable
  uint8_t* ram;

  // up to 64K of rom at 0x100000
  // Readable, executable
  uint8_t* rom;

  // Size of the rom
  size_t rom_size;

  // Stack on which everything operates
  ia_arr$(vm_stack_val_t) stack;
  
  // If the processor was halted
  // (interrupts will un-halt it)
  bool is_halted;

  /// \brief Should the main thread die?
  ///
  /// Set if we get an exception inside
  /// the exception handling interrupt.
  /// In that case we have nothing to do.
  bool should_die;

  //---- Interrupts

  // If all interrupts are disabled
  bool interrupts_disabled;

  // Addresses of all the interrupt handlers
  vm_stack_val_t interrupt_vector[NUM_INTERRUPTS];

  // Type of the current interrupt
  // (top item is currently handled)
  ia_arr$(vm_interrupt_type) interrupt_type;

  // Return addresses of the interrupts
  ia_arr$(vm_stack_val_t) interrupt_return_addr;

  // Each time we ask an interrupt, it is placed here
  ia_arr$(vm_interrupt) asked_interrupts;

  //---- Exception handling

  vm_stack_val_t exception_pc;
  vm_stack_val_t exception_segfault_addr;
  vm_exception_type exception_type;

  //---- Keyboard
  uint16_t kb_scancode;
  uint8_t kb_action;
  uint8_t kb_mods;

  //---- Showing messages

  /// Function to log a message
  void (*log_fn)(vm_log_type type, const char* fmt, ...);

  /// Print one character on the printer
  void (*printer_putc)(char c);

  //---- The CRT
  
  struct vm_crt* crt;

  vm_stack_val_t crt_x, crt_y;

  //---- V2

  vm_stack_val_t v2_sp, v2_sf;

} vm_state;

/// \brief Initialize a new VM 
vm_state* vm_state_new (void);

/// \brief Free a VM you no longer need
/// Remember to kill all the threads which use the state before this!
void vm_state_destroy(vm_state* state);

/// \brief Trigger an interrupt
void vm_state_trigger_interrupt(vm_state* state, vm_interrupt intr);

/// \brief Halt the machine
///
/// This function sets all things in state and waits untill
/// an interrupt is trigerred. Then it returns.
/// 
/// \hack HACK!!!: This unlocks/locks state mutex twice,
///                because the first time it is locked by the main loop.
///                A better way is needed, but I couldn't find a way
///                to read lock count of the recursive mutex.
///                A base library with all the threading stuff is required.
///
void vm_state_halt(vm_state* state);

/// \brief Change the ROM
///
/// If code is written for it, ROM can be even hot-swapped,
/// because you can load all important stuff into RAM and
/// execute it from there.
///
void vm_state_mount_rom(vm_state* state, uint8_t* rom, size_t rom_size);

/// \brief Raise an exception to be handled with corresponding interrupt
void vm_state_raise_exception(vm_state* state, vm_exception_type type);

const char* vm_strerr(vm_exception_type exc);

void vm_log_error(vm_state* state);

#endif
