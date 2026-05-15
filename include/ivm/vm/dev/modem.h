#ifndef I_IVM_DEV_MODEM
#define I_IVM_DEV_MODEM
#include "ivm/vm/dev.h"
#include <stdint.h>

#define VM_MDM_S_LINE 0x01
#define VM_MDM_S_CONN 0x02
#define VM_MDM_S_INCM 0x04
#define VM_MDM_S_FAIL 0x08

#define VM_MDM_C_CONN 0x01
#define VM_MDM_C_QUIT 0x02
#define VM_MDM_C_SEND 0x03
#define VM_MDM_C_RECV 0x04
#define VM_MDM_C_FCLR 0x05 // clear fail bit

#define VM_MDM_E_NONE      0x00
#define VM_MDM_E_BADCMD    0x01
#define VM_MDM_E_NOLINE    0x02
#define VM_MDM_E_CONNFAIL  0x03
#define VM_MDM_E_CONNRESET 0x04
#define VM_MDM_E_SEGFAULT  0x05
#define VM_MDM_E_BADADDR   0x06

typedef struct vm_modem {
    vm_dev dev;
    vm_state *vm;
    int fd;
    uint64_t arg_a, arg_b; // arguments
    uint8_t status, error;
} vm_modem;

void vm_modem_init(vm_modem *modem, vm_state *vm);
void vm_modem_update(vm_modem *modem);
void vm_modem_destroy(vm_modem *modem);

#endif
