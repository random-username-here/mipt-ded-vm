#include "ivm/vm/dev/printer.h"
#include "ivm/vm/dev.h"
#include "ivm/vm/mem.h"
#include <ctype.h>

static char xdigits[] = "0123456789abcdef";

static bool _write_printer(
    vm_state *state,
    vm_dev *self,
    vm_stack_val_t off,
    uint8_t val
) {
    if (off != 0)
        return vm_mem_segfault(state, off + self->base);
    vm_printer *pn = (vm_printer*) self;
    if (pn->callback) {
        if (isgraph(val) || isspace(val)) {
            pn->callback((char) val);
        } else {
            pn->callback('\\');
            pn->callback('x');
            pn->callback(xdigits[val / 16]);
            pn->callback(xdigits[val % 16]);
        }
    }
    return true;
}

void vm_printer_init(vm_printer *pn)
{
    *pn = (vm_printer) {
        .dev = (vm_dev) {
            .type = VM_DEV_PRINTER,
            .name = "Emulated printer to tty",
            .size = 16,
            .writer = _write_printer,
            .reader = NULL
        }
    };
}
