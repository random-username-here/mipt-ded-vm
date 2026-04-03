#include "ivm/asm/arch/ivm.h"
#include "ivm/asm/asm.h"
#include "ivm/asm/ast.h"
#include "ivm/common/array.h"
#include "ivm/common/parser/span.h"
#include "ivm/asm/symtab.h"
#include "ivm/common/macros.h"
#include "ivm/common/util.h"

#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <stdlib.h>

void _print_span(ivm_span span) {
  for (size_t i = 0; i < ivm_span_get_length(span); ++i)
    putchar(ivm_span_get_char(span, i));
}

size_t _inst_len(ivm_span name, ivm_span args) {
  return ivm_span_get_length(name)+1;
}

void _emit_instr(ivm_span name, ivm_span args, ivm_symtab *symtab, FILE* out) {
  ivm_span_fprint(out, name);
  fputc('\n', out);
}

int main (int argc, const char** argv) {


  if (argc == 2 && !strcmp(argv[1], "--help")) {
    fprintf(stderr, "Usage: %s [input file] [output]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "TODO: add docs here\n");
    return 0;
  }

  if (argc < 3) {
    fprintf(stderr, "Read --help\n");
    return EX_USAGE;
  }

  FILE* out = fopen(argv[2], "wb");
  check$(out, "You should provide a valid file to output bytecode into");

  iasm_assemble(out, argv[1], iasm_ivm_inst_len, iasm_ivm_emit_instr);

  return 0;
}
