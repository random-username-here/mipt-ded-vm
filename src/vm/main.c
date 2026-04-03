#include "ivm/common/macros.h"
#include "ivm/common/util.h"
#include "ivm/vm/crt.h"
#include "ivm/vm/mainloop.h"
#include "ivm/vm/state.h"
#include <bits/time.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#define OPS_PER_UPDATE 1000
#define TIMER1_TIME_NS (1000 * 1000 * 1000 / 30.0) // 10 ms

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
    fputs(ESC_RED "\n\\ " ESC_RST, stdout);
    printed_nl = true;
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

static void l_timer1_reset(vm_state *vm, void *arg) {
    *((bool*) arg) = false;
}

void vm_timer1_loop(vm_state *vm) {
    static uint64_t prev_t = 0;
    static bool is_waiting = false;
    if (is_waiting) return;
    uint64_t cur_t = vm_time_ns();
    if (prev_t + TIMER1_TIME_NS > cur_t) return;
    prev_t = cur_t;
    vm_state_trigger_interrupt(vm, (vm_interrupt) {
        .type = VM_INTR_TIMER1,
        .setup_state = l_timer1_reset,
        .data = &is_waiting
    });
    is_waiting = true;
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

    vm_crt *crt = vm_crt_new(vm);

    while (!vm->should_die) {  
        for (size_t i = 0; i < OPS_PER_UPDATE; ++i)
            vm_exec(vm);
        vm_crt_loop(crt);
        vm_timer1_loop(vm);
    }

    vm_crt_destroy(crt);

    log_fn(VM_LOG_INFO, "VM main loop exited");

    return 0;
}
