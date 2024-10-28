#include "ivm/common/macros.h"
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <sysexits.h>
#include <stdlib.h>

__attribute__((noreturn))
void _ivm_common_die(const char* file, size_t line, const char* fmt, ...) {

  fprintf(stderr, ESC_RED ESC_BOLD "\nPanic at " ESC_UNDERLINE "%s:%zu\n\n" ESC_RST, file, line);

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n\n");

  raise(SIGABRT);
  // If raise did not crash for good?
  exit(EX_SOFTWARE);
}
