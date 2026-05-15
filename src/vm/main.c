#include "ivm/common/macros.h"
#include "ivm/common/util.h"
#include "ivm/vm/dev/crt.h"
#include "ivm/vm/dev/modem.h"
#include "ivm/vm/dev/printer.h"
#include "ivm/vm/mainloop.h"
#include "ivm/vm/state.h"
#include <bits/time.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#define OPS_PER_UPDATE 1000
#define TIMER1_TIME_NS (1000 * 1000 * 1000 / 30.0) // 10 ms

const char* prefixes[] = {
  [VM_LOG_VERB] = ESC_GRAY      "Verbose : " ESC_RST,
  [VM_LOG_ERROR] = ESC_RED      "Error   : " ESC_RST,
  [VM_LOG_WARNING] = ESC_YELLOW "Warning : " ESC_RST,
  [VM_LOG_INFO] = ESC_GREEN     "Note    : " ESC_RST
};

bool printed_nl = true;
bool last_was_printer = false;

void log_fn(vm_log_type type, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  last_was_printer = false;
  if (!printed_nl) {
    fputs(ESC_RED "\\\n" ESC_RST, stdout);
    printed_nl = true;
  }
  fputs(prefixes[type], stdout);
  vprintf(fmt, args);
  fputs("\n", stdout);
  va_end(args);
}

void printer_putc(char c) {
    if (!last_was_printer || printed_nl) {
        fputs(ESC_BLUE "Printer : " ESC_RST, stdout);
        last_was_printer = true;
        printed_nl = false;
    }
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

void help(void) {
    puts("ivm -- An emulator for same-named ISA");
    puts("Usage: ivm [OPTS] ROM_FILE");
    puts("Options:");
    puts("  -h  Show this help message");
    puts("  -v  Verbosely print executed instructions/addresses");
    puts("  -c  Enable CRT/keyboard emulation via raylib");
    puts("  -m  Enable modem emulation via wrapped socket");
    puts("This version supports ivm v2");
}

int main (int argc, char* argv[]) {

    int opt;
    bool have_video = false, have_modem = false;
    bool verbose_log = false;

    while ((opt = getopt(argc, argv, "hcvm")) != -1) {
        switch (opt) {
            case 'h': help(); exit(0);
            case 'c': have_video = true; break;
            case 'v': verbose_log = true; break;
            case 'm': have_modem = true; break;
            default:
                printf("Unknown argument `%c`, consult `-h`\n", opt);
                exit(1);
        }
    }

    if (optind != argc-1) {
        puts("Expected only one positional argument -- ROM file");
        exit(1);
    }

    size_t rom_size;
    char* rom;
    if(!read_binary_file(argv[optind], &rom_size, &rom))
        die$("Failed to read file `%s` with ROM: %s", argv[1], strerror(errno));

    vm_state vm;
    vm_state_init(&vm, log_fn);
    vm.pc = RAM_END;
    vm.verbose_log = verbose_log;

    vm_printer printer;
    vm_printer_init(&printer);
    printer.callback = printer_putc;
    vm_state_mount_dev(&vm, &printer.dev, 0x120);

    vm_crt crt;
    if (have_video) {
        vm_crt_init(&crt, &vm);
        vm_state_mount_dev(&vm, &crt.dev, 0x130);
        vm_state_mount_dev(&vm, &crt.kbd, 0x150);
    } else {
        log_fn(VM_LOG_INFO, "CRT and keyboard are not enabled (use -c to enable them)");
    }

    vm_modem modem;
    if (have_modem) {
        vm_modem_init(&modem, &vm);
        vm_state_mount_dev(&vm, &modem.dev, 0x160);
    } else {
        log_fn(VM_LOG_INFO, "Modem is not enabled (use -m to enable it)");
    }

    if (verbose_log)
        log_fn(VM_LOG_INFO, "Verbose logs enabled, prepare to read a hundred pages of logs");

    vm_state_mount_rom(&vm, (uint8_t*) rom, rom_size);
    log_fn(VM_LOG_INFO, "Mounted %zu bytes of ROM from %s", rom_size, argv[optind]);
    
    log_fn(VM_LOG_INFO, "VM ready, starting");

    while (!vm.should_die) {  
        for (size_t i = 0; i < OPS_PER_UPDATE; ++i)
            vm_exec(&vm);
        if (have_video)
            vm_crt_loop(&crt);
        if (have_modem)
            vm_modem_update(&modem);
        vm_timer1_loop(&vm);
    }

    if (have_video)
        vm_crt_destroy(&crt);
    if (have_modem)
        vm_modem_destroy(&modem);
    vm_state_destroy(&vm);

    log_fn(VM_LOG_INFO, "VM main loop exited");

    return 0;
}
