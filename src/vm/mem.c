#include "ivm/vm/mem.h"
#include "ivm/vm/state.h"
#include "ivm/vm/dev.h"
#include <endian.h>
#include <stdint.h>
#include <stdio.h>


/// By default everything there has NULL read/write handlers
/// which raise segfault.
static vm_dev *sys_seg_handlers[SYSMEM_END] = { 0 };

void vm_state_mount_dev(vm_state *state, vm_dev *dev, vm_stack_val_t base)
{
    state->log_fn(VM_LOG_INFO, "Mounted device `%s` at $%x - $%x", dev->name, base, base + dev->size);
    ia_push$(&state->devices, dev);
    dev->base = base;
    for (vm_stack_val_t i = 0; i < dev->size; ++i)
        sys_seg_handlers[base + i] = dev;
}

bool vm_mem_segfault(vm_state *state, vm_stack_val_t addr)
{
    state->exception_segfault_addr = addr;
    vm_state_raise_exception(state, VM_EXC_SEGFAULT);
    return false;
}

uint8_t* index_num(vm_stack_val_t* val, size_t index) 
{
  check$(index < sizeof(vm_stack_val_t), "Index must be witn bounds of stack value");

  int n = 1;
  if (*(uint8_t*)&n == 1) {
    // We are on little endian machine
    return ((uint8_t*) val) + index;
  } else {
    // Big endian machine
    return ((uint8_t*) val) + sizeof(vm_stack_val_t) - 1 - index;
  }  
}

uint8_t index_num_const(vm_stack_val_t val, size_t index)
{
  return *index_num(&val, index);
}

static bool _read_byte(
      vm_state* state,
      vm_stack_val_t addr,
      vm_mem_usage usage, uint8_t* out
    ) {

  // everything uses multibyte reads anyways
  //if (state->verbose_log)
  //  state->log_fn(VM_LOG_VERB, "Read $%05llx", addr);

  //printf("Read %p\n", addr);

  if (addr < SYSMEM_END) {
    vm_dev *dev = sys_seg_handlers[addr];
    if (!dev || !dev->reader)
        return vm_mem_segfault(state, addr);
    return dev->reader(state, dev, addr - dev->base, usage, out);
  } else if (addr < RAM_END) {
    *out = state->ram[addr - SYSMEM_END];
  } else if (addr < RAM_END + (vm_stack_val_t) state->rom_size) {
    *out = state->rom[addr - RAM_END]; 
  } else {
      return vm_mem_segfault(state, addr);
  }

  return true;
}

bool vm_mem_read(
      vm_state* state,
      vm_stack_val_t addr, size_t size,
      vm_mem_usage usage, vm_stack_val_t* out
    ) {

  *out = 0;

  for (size_t i = 0; i < size; ++i) {
    uint8_t byte;
    if (!_read_byte(state, addr + i, usage, &byte))
      return false;
    *out |= ((vm_stack_val_t) byte) << (i*8);
  }

  if (state->verbose_log && usage != VM_MEM_EXEC)
    state->log_fn(VM_LOG_VERB, "Read $%05llx[0..%d] -> $%llx", addr, size, *out);

  return true;
}

static bool _write_byte(
    vm_state* state, vm_stack_val_t addr,
    uint8_t val
) {
  if (addr < 0)
      return vm_mem_segfault(state, addr);
  
  //if (state->verbose_log)
  //    state->log_fn(VM_LOG_VERB, "Write $%05llx <- $%02x", addr, val);

  if (addr < SYSMEM_END) {
    vm_dev *dev = sys_seg_handlers[addr];
    if (!dev || !dev->writer)
        return vm_mem_segfault(state, addr);
    return dev->writer(state, dev, addr - dev->base, val);
  } else if (addr < RAM_END) {
    state->ram[addr - SYSMEM_END] = val;
  } else {
      return vm_mem_segfault(state, addr);
  }

  return true;
}

bool vm_mem_write(vm_state* state, vm_stack_val_t addr, size_t size, vm_stack_val_t val)
{
  if (state->verbose_log)
    state->log_fn(VM_LOG_VERB, "Write $%05llx[0..%d] <- $%llx", addr, size, val);
  for (size_t i = 0; i < size; ++i) {
    if (!_write_byte(state, addr + i, (val >> (i*8)) & 0xff))
      return false;
  }
  return true;
}
