/// \file
/// \brief Memory handling in IVM
///
/// Functions defined here give you access to stuff in
/// address space. They can raise `VM_EXC_SEGFAULT` if
/// something is wrong.
///
/// Note what you can read any consecutive non-segfaulting
/// addresses, so read last 4 bytes of interrupt 0x1f,
/// exception type and first 3 bytes of exception address
/// if you want to in one `get64`. Nothing prevents you
/// from doing this.
///

#ifndef IVM_VM_MEM
#define IVM_VM_MEM

#include "ivm/vm/state.h"

typedef enum {
  VM_MEM_READ = 1,
  VM_MEM_WRITE = 2,
  VM_MEM_EXEC = 4
} vm_mem_usage;


/// \brief Read little-endian number of N bytes at specified address
///
/// \param state State of the VM
/// \param addr Address to read
/// \param size Number of bytes to read
/// \param usage `VM_MEM_READ` or `VM_MEM_EXEC` depending on
///              what you need this memory for.
/// \param [out] out Output for the result
/// \returns `true` if no `SEGFAULT` was raised
///
bool vm_mem_read(
    vm_state* state,
    vm_stack_val_t addr, size_t size,
    vm_mem_usage usage, vm_stack_val_t* out
  );


/// \brief Write the value into the memory in little-endian format
///
/// \param state State of the machine
/// \param addr Address to start writing from
/// \param size Number of bytes to write
/// \param val Value to write. If it is bigger than `size`,
///            only `size` lowest bytes will be written.
///
bool vm_mem_write(vm_state* state, vm_stack_val_t addr, size_t size, vm_stack_val_t val);

#endif
