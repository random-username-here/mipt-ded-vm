#include "ivm/vm/mem.h"
#include "ivm/vm/crt.h"
#include "ivm/vm/state.h"
#include <endian.h>
#include <stdint.h>
#include <stdio.h>

typedef bool (*read_handler)(
                    vm_state* state,
                    vm_stack_val_t addr, vm_mem_usage usage,
                    uint8_t* out
              );

typedef bool (*write_handler)(
                    vm_state* state,
                    vm_stack_val_t addr,
                    uint8_t val
              );

/// Description of one byte in 4K of system memory
/// (each address there can do complex stuff
/// when peeked/poked)
typedef struct {
  read_handler read;
  write_handler write;
} sys_seg_descr; 


/// By default everything there has NULL read/write handlers
/// which raise segfault.
static sys_seg_descr sys_seg_handlers[SYSMEM_END] = { 0 };


//=====[ The system segment handlers ]==========================================

//-----[ Helper functions ]-----------------------------------------------------

#define BAD_ACCESS() do {\
    state->exception_segfault_addr = addr;\
    vm_state_raise_exception(state, VM_EXC_SEGFAULT);\
    return false;\
  } while(0)

static uint8_t* _index_num(vm_stack_val_t* val, size_t index) {

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

static uint8_t _index_num_const(vm_stack_val_t val, size_t index) {
  return *_index_num(&val, index);
}

//-----[ Interrupt vector ]-----------------------------------------------------

static bool _read_interrupt_vector(
      vm_state* state,
      vm_stack_val_t addr, vm_mem_usage usage,
      uint8_t* out
    ) {

  check$(8 <= addr && addr < 8 + NUM_INTERRUPTS * 8, "Address must be withn the interrupt vector");

  if (usage != VM_MEM_READ)
    BAD_ACCESS();

  *out = _index_num_const(state->interrupt_vector[addr / 8 - 1], addr % 8);

  return true;
}

static bool _write_interrupt_vector(
      vm_state* state,
      vm_stack_val_t addr,
      uint8_t val
    ) {

  check$(0 <= addr && addr < NUM_INTERRUPTS * 8, "Address must be withn the interrupt vector");

  *_index_num(&state->interrupt_vector[addr / 8], addr % 8) = val;

  return true;
}

//-----[ Exception data ]-------------------------------------------------------

static bool _read_exception_data(
      vm_state* state,
      vm_stack_val_t addr, vm_mem_usage usage,
      uint8_t* out
    ) {

  check$(0x100 <= addr && addr < 0x120, "Address must be withn exception data space");

  if (usage != VM_MEM_READ)
    BAD_ACCESS();

  if (addr == 0x100)
    *out = state->exception_type;
  else if (0x101 <= addr && addr <= 0x108)
    *out = _index_num_const(state->exception_pc, addr - 0x101);
  else if (0x109 <= addr && addr <= 0x111)
    *out = _index_num_const(state->exception_segfault_addr, addr - 0x109);
  else
    BAD_ACCESS();

  return true;
}

//-----[ Printer ]--------------------------------------------------------------

static bool _write_printer(
      vm_state* state,
      vm_stack_val_t addr,
      uint8_t val
    ) {

  if (state->printer_putc)
    state->printer_putc((char) val);

  return true;
}

//-----[ CRT ]------------------------------------------------------------------

static bool _write_crt(
    vm_state* state,
    vm_stack_val_t addr,
    uint8_t val
  ) {
  if (addr == 0x00134) {
    // TODO: CRT_SIZE
    if (val == 0)
      vm_crt_goto(state->crt, state->crt_x / 65535.0, state->crt_y / 65535.0);
    else
      vm_crt_lineto(state->crt, state->crt_x / 65535.0, state->crt_y / 65535.0);
  } else if (addr == 0x00130 || addr == 0x00131) {
    *_index_num(&state->crt_x, addr - 0x00130) = val;
  } else if (addr == 0x00132 || addr == 0x00133) {
    *_index_num(&state->crt_y, addr - 0x00132) = val;
  } else {
    BAD_ACCESS();
  }

  return true;
}

//-----[ Keyboard ]-------------------------------------------------------------

static bool _read_keyboard(
    vm_state* state,
    vm_stack_val_t addr, vm_mem_usage usage,
    uint8_t* out
  ) {

  if (usage != VM_MEM_READ)
    BAD_ACCESS();

  if (addr == 0x00150 || addr == 0x00151)
    *out = _index_num_const(state->kb_scancode, addr - 0x00150);
  else if (addr == 0x00152)
    *out = state->kb_action;
  else if (addr == 0x00153)
    *out = state->kb_mods;
  else
    BAD_ACCESS();

  return true;
}

//=====[ System segment initialization ]========================================

static void _assign_fn(size_t begin, size_t end, read_handler read_fn, write_handler write_fn) {
  for (size_t i = begin; i < end; ++i)
    sys_seg_handlers[i] = (sys_seg_descr) { .write = write_fn, .read = read_fn };
}

__attribute__((constructor))
static void _init_sys_segment(void) {
  _assign_fn(0x0008, 0x0100, _read_interrupt_vector, _write_interrupt_vector);
  _assign_fn(0x0100, 0x0120, _read_exception_data, NULL);
  _assign_fn(0x0120, 0x0130, NULL, _write_printer);
  _assign_fn(0x0130, 0x0150, NULL, _write_crt);
  _assign_fn(0x0150, 0x0160, _read_keyboard, NULL);
}

//=====[ Implementations ]======================================================

static bool _read_byte(
      vm_state* state,
      vm_stack_val_t addr,
      vm_mem_usage usage, uint8_t* out
    ) {

  //printf("Read %p\n", addr);

  if (addr < SYSMEM_END) {
    read_handler hndl = sys_seg_handlers[addr].read;

    if (!hndl)
      BAD_ACCESS();
    
    return hndl(state, addr, usage, out);

  } else if (addr < RAM_END) {
    *out = state->ram[addr - SYSMEM_END];
  } else if (addr < RAM_END + (vm_stack_val_t) state->rom_size) {
    *out = state->rom[addr - RAM_END]; 
  } else
    BAD_ACCESS();

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

  //printf("Read multibyte %p[0..%d] = %lld\n", addr, size, *out);

  return true;
}


static bool _write_byte(
      vm_state* state, vm_stack_val_t addr,
      uint8_t val
    ) {
  
  //printf("Write %p = %d\n", addr, val);

  if (addr < SYSMEM_END) {
    write_handler hndl = sys_seg_handlers[addr].write;
   
    if (!hndl)
      BAD_ACCESS();

    return hndl(state, addr, val);

  } else if (addr < RAM_END) {
    state->ram[addr - SYSMEM_END] = val;
  } else
    BAD_ACCESS();

  return true;
}


bool vm_mem_write(vm_state* state, vm_stack_val_t addr, size_t size, vm_stack_val_t val) {

  for (size_t i = 0; i < size; ++i) {
    if (!_write_byte(state, addr + i, (val >> (i*8)) & 0xff))
      return false;
  }

  return true;
}


#undef BAD_ACCESS

