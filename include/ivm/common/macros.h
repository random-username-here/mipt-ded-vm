
#ifndef IVM_COMMON_MACROS
#define IVM_COMMON_MACROS

#include <stddef.h>

// Error checking
__attribute__((noreturn))
void _ivm_common_die(const char* file, size_t line, const char* fmt, ...);

#define die$(...) _ivm_common_die(__FILE__, __LINE__, __VA_ARGS__) 
#define check$(condition, ...) do { if(!(condition)) die$(__VA_ARGS__); } while(0)

// Colors
#define ESC_RST       "\x1b[0m"
#define ESC_BOLD      "\x1b[1m"
#define ESC_UNDERLINE "\x1b[4m"
#define ESC_GRAY      "\x1b[90m"
#define ESC_RED       "\x1b[91m"
#define ESC_YELLOW    "\x1b[93m"
#define ESC_BLUE      "\x1b[94m"
#define ESC_GREEN     "\x1b[92m"

// Concat macro
#define _concat$(a, b) a ## b
#define concat$(a, b) _concat$(a, b)

#endif
