
#ifndef IVM_COMMON_UTIL
#define IVM_COMMON_UTIL

#include <stdbool.h>
#include <stddef.h>

/// \brief Read a file 

bool read_file(const char* filename, size_t* size_out, char** data_out);

bool read_binary_file(const char* filename, size_t* size_out, char** data_out);

#endif
