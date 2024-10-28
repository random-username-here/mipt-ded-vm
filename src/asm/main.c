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

  if (argc < 3) {
    fprintf(stderr, "Read --help\n");
    return EX_USAGE;
  }

  if (!strcmp(argv[1], "--help")) {
    fprintf(stderr, "Usage: %s [input file]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "TODO: add docs here\n");
    return 0;
  }


  FILE* out = fopen(argv[2], "wb");
  check$(out, "You should provide a valid file to output bytecode into");

  char* buf;
  if (!read_file(argv[1], NULL, &buf))
    die$("Failed to read input file");

  ivm_file* f = ivm_file_new(argv[1], buf);
  check$(f, "File must be created");

  ia_arr$(ast_node) ast = iasm_parse_file(f);
  //iasm_dump_ast(ast);
  //fflush(stdout);

  iasm_assemble(out, ast, iasm_ivm_inst_len, iasm_ivm_emit_instr);

  ia_destroy_array(ast);

  /*const char test[] = 
    "; A test thing.\n"
    "; Note what execution order is not defined\n"
    "c = b / a;\n"
    "a = 1 + 2;\n"
    "b = a * 4;\n"
    "d = 4 ^ 2;";*/

  /*regex_t not_semicolon;
  ivm_compile_regex_or_die(&not_semicolon, "^[^;\n]*", REG_EXTENDED);



  ivm_span span = ivm_file_full_span(f);

  ivm_report(REPORT_NOTE, span, "The whole input file");
  ivm_report_add_note("This is not related to any errors");

  ivm_symtab symtab = ivm_symtab_new();

  while (!ivm_span_is_empty(span)) {
    span = ivm_trim_ws_and_comments(span);
    if (ivm_span_is_empty(span))
      break;

    ivm_span name;
    span = ivm_match_identifier(span, &name);

    span = ivm_trim_ws_and_comments(span);
    if (ivm_span_is_empty(span) || ivm_span_get_char(span, 0) != '=') {
      ivm_report(REPORT_ERROR, name, "Expected an assignment operator after the variable name");
      exit(EX_DATAERR);
    }
    span = ivm_span_slice(span, 1, ivm_span_slice_to_end);

    ivm_span expr;
    span = ivm_match_regex(span, &not_semicolon, &expr);

    if (ivm_span_is_empty(span) || ivm_span_get_char(span, 0) != ';') {
      ivm_report(REPORT_ERROR, ivm_span_slice(span, 0, 0), "Semicolon missing here");
      exit(EX_DATAERR);
    }
    span = ivm_span_slice(span, 1, ivm_span_slice_to_end);

    ivm_compute_expression(name, "constant", expr, &symtab, false);
  }

  ivm_symtab_compute_values(symtab);

  for (size_t i = 0; i < ia_length(symtab); ++i) {
    symtab[i].print_name(&symtab[i]);
    fprintf(stderr, ESC_GRAY " = " ESC_RST ESC_YELLOW "%lld\n" ESC_RST, symtab[i].value);
  }

  ivm_symtab_destroy(symtab);
  ivm_file_destroy(f);
  free(buf);*/

  return 0;
}
