#ifndef I_IVM_DEV_PRINTER
#define I_IVM_DEV_PRINTER
#include "ivm/vm/dev.h"

typedef struct vm_printer {
    vm_dev dev;
    void (*callback)(char ch);
} vm_printer;

void vm_printer_init(vm_printer *pn);

#endif
