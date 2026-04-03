/// \file
/// \brief Main thread program execution loop here
/// 

#ifndef IVM_VM_MAINLOOP
#define IVM_VM_MAINLOOP

#include "ivm/vm/state.h"

/// Execute stuff
void vm_exec(vm_state* state);

uint64_t vm_time_ns(void);

#endif
