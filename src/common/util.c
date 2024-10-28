#include "ivm/common/util.h"
#include "ivm/common/macros.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

static bool _read_file(const char* filename, const char* mode, size_t* size_out, char** data_out) {

  check$(filename, "Expected a file name to read");

  FILE* f = fopen(filename, mode);
  if (!f)
    return false;

  struct stat fs;
  if(fstat(fileno(f), &fs))
    return false;

  if (size_out)
    *size_out = fs.st_size+1;

  if (data_out) {
    char* buf = (char*) calloc(fs.st_size+1, 1);
    if (!buf)
      return false;
    fread(buf, 1, fs.st_size, f);
    if (ferror(f))
      return false;
    buf[fs.st_size] = 0;
    *data_out = buf;
  }

  if (fclose(f))
    return false;

  return true;
}

bool read_file(const char* filename, size_t* size_out, char** data_out) {
  return _read_file(filename, "r", size_out, data_out);
}

bool read_binary_file(const char* filename, size_t* size_out, char** data_out) {
  return _read_file(filename, "rb", size_out, data_out);
}
