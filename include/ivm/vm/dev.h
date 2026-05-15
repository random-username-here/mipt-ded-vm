///
/// \file
/// \brief IVM device manager
///
#ifndef I_IVM_DEV
#define I_IVM_DEV
#include <stdbool.h>
#include <stdint.h>
#include "ivm/vm/mem.h"

typedef bool (*vm_read_handler)(
        vm_state *state, vm_dev *dev,
        vm_stack_val_t offset, vm_mem_usage usage,
        uint8_t *out
);

typedef bool (*vm_write_handler)(
        vm_state* state, vm_dev *dev,
        vm_stack_val_t offset,
        uint8_t val
);

typedef enum vm_dev_type {
    VM_DEV_NONSTANDARD = 0,
    VM_DEV_CPUREGS = 1,
    VM_DEV_PRINTER = 2,
    VM_DEV_CRT = 3,
    VM_DEV_KEYBOARD = 4,
    VM_DEV_MODEM = 5
} vm_dev_type;

typedef struct vm_dev {
    vm_dev_type type;
    const char *name;
    vm_read_handler reader;
    vm_write_handler writer;
    vm_stack_val_t size;
    vm_stack_val_t base; // set after mount
} vm_dev;

#endif
