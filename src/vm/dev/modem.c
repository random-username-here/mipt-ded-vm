#include "ivm/vm/dev/modem.h"
#include "ivm/vm/dev.h"
#include "ivm/vm/mem.h"
#include "ivm/vm/state.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MAX_ADDR_LEN 256

static void l_get_ptr_and_bounds(vm_state *vm, uint64_t addr, uint8_t **ptr, uint8_t **end) {
    if (addr < SYSMEM_END) *ptr = *end = NULL;
    else if (addr < RAM_END) *ptr = vm->ram + (addr - SYSMEM_END), *end = vm->ram + (RAM_END - SYSMEM_END);
    else if (addr < RAM_END + vm->rom_size) *ptr = vm->rom + (addr - RAM_END), *end = vm->rom + vm->rom_size;
    else *ptr = *end = NULL;
}

static uint8_t *l_get_cstr(vm_state *vm, uint64_t addr) {
    uint8_t *res = NULL, *end = NULL;
    l_get_ptr_and_bounds(vm, addr, &res, &end);
    if (res == NULL) return NULL;
    for (size_t i = 0; i < MAX_ADDR_LEN && res + i < end; ++i)
        if (res[i] == '\0') return res;
    return NULL; // too long/not terminated
}

static void l_vm_modem_enter_error_state(vm_modem *modem, uint8_t err, bool soft) {
    if (err == VM_MDM_E_CONNRESET)
        modem->vm->log_fn(VM_LOG_WARNING, "Modem: connection closed by foreign host");
    if (!soft) {
        if (modem->fd != -1) close(modem->fd);
        modem->fd = -1;
        modem->status = VM_MDM_S_FAIL | VM_MDM_S_LINE;
    } else {
        modem->status |= VM_MDM_S_FAIL;
    }
    modem->error = err;
}

void vm_modem_update(vm_modem *modem)
{
    if (modem->fd == -1) return;
    // TODO: check if still online
    // TODO: check if socket closed
    char ch;
    bool incoming = true;
    int cnt = recv(modem->fd, &ch, 1, MSG_PEEK | MSG_DONTWAIT);
    if (cnt == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            incoming = false;
        } else if (errno == ECONNRESET) {
            l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNRESET, false);
        } else {
            modem->vm->log_fn(VM_LOG_ERROR, "Modem: recv(.., MSG_PEEK) failed: %s", strerror(errno));
            l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNFAIL, false);
        }
        return;
    } else if (cnt == 0) {
        cnt = recv(modem->fd, NULL, 0, MSG_PEEK | MSG_DONTWAIT);
        if (cnt == 0) {
            // shutdown
            l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNRESET, false);
        } else {
            incoming = false;
        }
    }

    bool was_incoming = (modem->status & VM_MDM_S_INCM) != 0;
    if (incoming != was_incoming) {
        modem->status ^= VM_MDM_S_INCM;
        if (incoming) {
            vm_state_trigger_interrupt(modem->vm, (vm_interrupt) {
                .type = VM_INTR_MODEM,
                .setup_state = NULL,
                .data = NULL
            });
        }
    }
}


static void l_vm_modem_connect(vm_state *vm, vm_modem *modem)
{
    if (modem->fd != -1) {
        vm->log_fn(VM_LOG_WARNING, "Modem: connecting again, not quitting previous connection");
        close(modem->fd);
        modem->fd = -1;
    }
    modem->status = VM_MDM_S_LINE;
    modem->error = VM_MDM_E_NONE;

    modem->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (modem->fd == -1) {
        vm->log_fn(VM_LOG_WARNING, "Modem: could not create socket: %s", strerror(errno));
        l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNFAIL, false);
        return;
    }

    const char *name = (const char*) l_get_cstr(vm, modem->arg_a);
    if (!name) {
        vm->log_fn(VM_LOG_WARNING, "Modem: could not read name from arg A");
        l_vm_modem_enter_error_state(modem, VM_MDM_E_SEGFAULT, false);
        return;
    }
    
    vm->log_fn(VM_LOG_INFO, "Modem: will be connecting to `%s:%d`", name, modem->arg_b);
    struct hostent *host = gethostbyname(name);
    if (!host) {
        vm->log_fn(VM_LOG_WARNING, "Modem: failed to resolve address");
        l_vm_modem_enter_error_state(modem, VM_MDM_E_BADADDR, false);
        return;
    }
    vm->log_fn(VM_LOG_INFO, "Modem: gethostbyname() gave official name `%s`", host->h_name);

    if (host->h_addrtype != AF_INET) {
        vm->log_fn(VM_LOG_WARNING, "Modem: no IPv4 found");
        l_vm_modem_enter_error_state(modem, VM_MDM_E_BADADDR, false);
        return;
    }
    if (host->h_length == 0) {
        vm->log_fn(VM_LOG_WARNING, "Modem: IP list empty");
        l_vm_modem_enter_error_state(modem, VM_MDM_E_BADADDR, false);
        return;
    }

    struct in_addr **ips = (struct in_addr**) host->h_addr_list;

    char s_addr[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, ips[0], s_addr, INET_ADDRSTRLEN);
    vm->log_fn(VM_LOG_INFO, "Modem: address is %s", s_addr);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, &ips[0]->s_addr, sizeof(struct in_addr));
    addr.sin_port = htons(modem->arg_b);

    vm->log_fn(VM_LOG_INFO, "Modem: connecting...");
    if (connect(modem->fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        vm->log_fn(VM_LOG_WARNING, "Modem: connect() failed: %s", strerror(errno));
        l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNFAIL, false);
        return;
    }
    
    modem->status |= VM_MDM_S_CONN;
    vm->log_fn(VM_LOG_INFO, "Modem: connected!");
}

static void l_vm_modem_send(vm_state *vm, vm_modem *modem)
{
    if (modem->fd == -1) {
        l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNFAIL, false);
        modem->arg_b = 0;
        return;
    }

    uint8_t *data = NULL, *end = NULL;
    l_get_ptr_and_bounds(vm, modem->arg_a, &data, &end);
    if (data + modem->arg_b >= end) {
        l_vm_modem_enter_error_state(modem, VM_MDM_E_SEGFAULT, true);
        modem->arg_b = 0;
        return;
    }

    int res = send(modem->fd, data, modem->arg_b, MSG_NOSIGNAL);
    if (res < 0) {
        vm->log_fn(VM_LOG_WARNING, "Modem: send() failed: %s", strerror(errno));
        if (errno == EPIPE || errno == ECONNRESET)
            l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNRESET, false);
        else
            l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNFAIL, false);
        modem->arg_b = 0;
    } else {
        modem->arg_b = res;
    }
}

static void l_vm_modem_recv(vm_state *vm, vm_modem *modem)
{
    if (modem->fd == -1) {
        l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNFAIL, false);
        modem->arg_b = 0;
        return;
    }

    uint8_t *data = NULL, *end = NULL;
    l_get_ptr_and_bounds(vm, modem->arg_a, &data, &end);
    if (data + modem->arg_b >= end) {
        l_vm_modem_enter_error_state(modem, VM_MDM_E_SEGFAULT, true);
        modem->arg_b = 0;
        return;
    }

    vm_modem_update(modem);
    if (!(modem->status & VM_MDM_S_INCM)) {
        modem->arg_b = 0;
        return;
    }
    modem->status &= ~VM_MDM_S_INCM;

    int count = recv(modem->fd, data, modem->arg_b, 0);
    if (count == -1) {
        vm->log_fn(VM_LOG_WARNING, "Modem: recv() failed: %s", strerror(errno));
        l_vm_modem_enter_error_state(modem, VM_MDM_E_CONNFAIL, false);
        modem->arg_b = 0;
    } else {
        modem->arg_b = count;
    }
}

static void l_vm_modem_cmd(vm_state *vm, vm_modem *modem, uint8_t cmd)
{
    switch (cmd) {
        case VM_MDM_C_CONN: // connect
            l_vm_modem_connect(vm, modem);
            return;
        case VM_MDM_C_QUIT: // disconnect
            if (modem->fd != -1) {
                vm->log_fn(VM_LOG_INFO, "Modem disconnecting");
                close(modem->fd);
                modem->fd = -1;
            }
            modem->status = VM_MDM_S_LINE;
            modem->error = VM_MDM_E_NONE;
            return;
        case VM_MDM_C_SEND:
            l_vm_modem_send(vm, modem);
            return;
        case VM_MDM_C_RECV:
            l_vm_modem_recv(vm, modem);
            return;
        case VM_MDM_C_FCLR:
            modem->status &= ~VM_MDM_S_FAIL;
            return;
        default:
            modem->error = VM_MDM_E_BADCMD;
            modem->status |= VM_MDM_S_FAIL;
            return;
    }
}

static bool l_vm_modem_write(
    vm_state *state, vm_dev *dev,
    vm_stack_val_t off, uint8_t val
) {
    vm_modem *modem = (vm_modem*) dev;
    switch (off) {
        case 0x0 ... 0x7: *index_num(&modem->arg_a, off) = val; break;
        case 0x8 ... 0xf: *index_num(&modem->arg_b, off - 0x8) = val; break;
        case 0x11: l_vm_modem_cmd(state, modem, val); break;
        default: return vm_mem_segfault(state, off + dev->base);
    }
    return true;
}

static bool l_vm_modem_read(
    vm_state *state, vm_dev *dev,
    vm_stack_val_t off, vm_mem_usage usage, uint8_t* out
) {
    if (usage != VM_MEM_READ)
        return vm_mem_segfault(state, off + dev->base);
    vm_modem *modem = (vm_modem*) dev;
    switch (off) {
        case 0x0 ... 0x7: *out = index_num_const(modem->arg_a, off); break;
        case 0x8 ... 0xf: *out = index_num_const(modem->arg_b, off - 0x8); break;
        case 0x10: vm_modem_update(modem); *out = modem->status; break;
        case 0x12: *out = modem->error; break;
        default: return vm_mem_segfault(state, off + dev->base);
    }
    return true;
}

void vm_modem_init(vm_modem *modem, vm_state *vm)
{
    *modem = (vm_modem) {
        .dev = (vm_dev) {
            .type = VM_DEV_MODEM,
            .name = "Emulated modem",
            .size = 32,
            .reader = l_vm_modem_read,
            .writer = l_vm_modem_write
        },
        .vm = vm,
        .fd = -1,
        .arg_a = 0, .arg_b = 0,
        .status = VM_MDM_S_LINE, .error = 0
    };
    vm_modem_update(modem);
}

void vm_modem_destroy(vm_modem *modem)
{
    if (modem->fd != -1) {
        close(modem->fd);
    }
}
