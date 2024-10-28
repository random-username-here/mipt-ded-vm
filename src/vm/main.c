#include "ivm/common/macros.h"
#include "ivm/common/util.h"
#include "ivm/vm/crt.h"
#include "ivm/vm/mainloop.h"
#include "ivm/vm/state.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <math.h>

const char* prefixes[] = {
  [VM_LOG_ERROR] = ESC_RED "Error: " ESC_RST,
  [VM_LOG_WARNING] = ESC_YELLOW "Warning: " ESC_RST,
  [VM_LOG_INFO] = ESC_GREEN "Note: " ESC_RST
};

atomic_bool printed_nl = true;

void log_fn(vm_log_type type, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (!printed_nl) {
    fputs(ESC_RED "\\\n" ESC_RST, stdout);
  }
  fputs(prefixes[type], stdout);
  vprintf(fmt, args);
  fputs("\n", stdout);
  va_end(args);
}

void printer_putc(char c) {
  if (c != '\0')
    printed_nl = c == '\n';
  putc(c, stdout);
}

void* interruptor(void* _vm) {
  vm_state* vm = (vm_state*) _vm;
  vm->log_fn(VM_LOG_INFO, "Interruptor thread started");

  while (!vm->should_die) {
    usleep(100000);
    vm->log_fn(VM_LOG_INFO, "Triggering timer interrupt...");
    vm_state_trigger_interrupt(_vm, VM_INTR_TIMER1);
  }

  return NULL;
}

int main (int argc, const char** argv) {
  
  if (argc != 2) {
    fprintf(stderr, "Go read the --help");
    return EX_OK;
  }

  if (!strcmp(argv[1], "--help")) {
    fprintf(stderr, "\n");
    fprintf(stderr, "ivm - A toy stack virtual machine\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: ivm <rom file>\n");
    fprintf(stderr, "\n");
    return EX_OK;
  }

  size_t rom_size;
  char* rom;
  if(!read_binary_file(argv[1], &rom_size, &rom))
    die$("Failed to read file `%s` with ROM: %s", argv[1], strerror(errno));

  vm_state* vm = vm_state_new();
  
  vm->printer_putc = printer_putc;
  vm->log_fn = log_fn;
  vm_state_mount_rom(vm, (uint8_t*) rom, rom_size);
  vm->pc = RAM_END;

  vm_crt* crt = vm_crt_new(vm);

  pthread_t timer_thread;
  pthread_create(&timer_thread, NULL, interruptor, vm); 
  vm_mainloop(vm);
  pthread_join(timer_thread, NULL);

  // Too nice to remove
  /*float t = 0;

  while (!crt->was_closed) {
    vm_crt_lineto(crt, sin(37 * t + M_PI*3/4) * 0.4 + 0.5, sin(29 * t) * 0.4 + 0.5);
    t += 0.001;
    usleep(1000 * 1000 / 60 / 10);
  }*/

  vm_crt_destroy(crt);

  log_fn(VM_LOG_INFO, "VM main loop exited");

  return 0;
}
