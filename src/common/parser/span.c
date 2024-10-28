#include "ivm/common/parser/span.h"
#include "ivm/common/macros.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>

//=====[ Methods for files ]====================================================

/// Setup the lines in given file
/// Returns `false` if failed to alloc memory
bool _setup_ivm_file(ivm_file* f) {

  check$(f, "Non-NULL file is required to setup lines in");

  f->num_lines = 1;
  for (const char* c = f->contents; *c; ++c)
    if (*c == '\n')
      f->num_lines++;

  f->line_start_offsets = (size_t*) calloc(f->num_lines, sizeof(size_t));
  if (!f->line_start_offsets)
    return false;

  f->line_start_offsets[0] = 0;
  size_t *curr_item = f->line_start_offsets+1;
  for (const char* c = f->contents; *c; ++c)
    if (*c == '\n')
      *(curr_item++) = (size_t) (c - f->contents + 1);

  return true;
}

// Here comes the goto land
// We need `defer` and `errdefer`, like in Zig. 

ivm_file* ivm_file_new(const char* name, const char* contents) {

  check$(name, "File name expected");
  check$(contents, "Some source code expected");

  ivm_file* f = (ivm_file*) calloc(1, sizeof(ivm_file));
  if (!f) goto fail_to_alloc_file;

  f->name = strdup(name);
  if (!f->name) goto fail_to_strdup_name;

  f->contents = strdup(contents);
  if (!f->contents) goto fail_to_strdup_contents;
  f->size = strlen(contents);

  if (!_setup_ivm_file(f)) goto fail_to_setup_lines;

  return f;

  // Error handling
 fail_to_setup_lines:
  free(f->contents);
 fail_to_strdup_contents:
  free(f->name);
 fail_to_strdup_name:
  free(f);
 fail_to_alloc_file:
  return NULL;
}


ivm_file* ivm_file_open(const char* filename) {
  
  check$(filename, "Non-NULL filename expected");

  FILE* file = fopen(filename, "r");
  if (!file) goto fail_to_open_file;

  struct stat file_stat;
  if (fstat(fileno(file), &file_stat))
    goto fail_to_stat;

  char* buf = (char*) calloc(file_stat.st_size+1, 1);
  if (!buf) goto fail_to_alloc_buffer;

  if (fread(buf, 1, file_stat.st_size, file) != file_stat.st_size) goto fail_to_read_file;

  ivm_file* f = (ivm_file*) calloc(1, sizeof(ivm_file));
  if (!f) goto fail_to_alloc_file;

  f->contents = buf;

  f->name = strdup(filename);
  if (!f->name) goto fail_to_strdup_name;

  if (!_setup_ivm_file(f)) goto fail_to_setup_lines;

  return f;

  // Handle errors
 fail_to_setup_lines:
  free(f->name);
 fail_to_strdup_name:
  free(f);
 fail_to_alloc_file:
 fail_to_read_file:
  free(buf);
 fail_to_alloc_buffer:
 fail_to_stat:
  fclose(file);
 fail_to_open_file:
  return NULL;
}

void ivm_verify_file(ivm_file* file) {
  
  check$(file, "Expected a file");
  check$(file->name, "Valid file must have a name");
  check$(file->contents, "Valid file must have contents");
  check$(file->line_start_offsets, "Valid file must have line offsets");

}

void ivm_file_destroy(ivm_file *file) {
  
  ivm_verify_file(file);
  
  free(file->name);
  free(file->contents);
  free(file->line_start_offsets);
  free(file);
}

ivm_span ivm_file_full_span(ivm_file* file) {

  ivm_verify_file(file);

  return (ivm_span) {
    .file = file,
    .begin = 0,
    .end = file->size
  };
}
//=====[ Methods for source locations ]=========================================

size_t ivm_sloc_get_line(ivm_sloc location) {

  ivm_verify_file(location.file);

  // Do a binsearch
  size_t l = 0, r = location.file->num_lines;
  while (r - l > 1) {
    size_t m = (l+r)/2;
    if (location.file->line_start_offsets[m] <= location.offset)
      l = m;
    else
      r = m;
  }

  return l;
}

size_t ivm_sloc_get_column(ivm_sloc location) {
  
  ivm_verify_file(location.file);

  return location.offset - location.file->line_start_offsets[ivm_sloc_get_line(location)];
}

//=====[ Methods for spans ]====================================================

ivm_span ivm_span_new(ivm_sloc begin, ivm_sloc end) {
  
  check$(begin.file == end.file, "Span begin and end must be in the same file");
  check$(begin.offset <= end.offset, "Span begin should be before its end");
  ivm_verify_file(begin.file);

  return (ivm_span) {
    .file = begin.file,
    .begin = begin.offset,
    .end = end.offset
  };
}

bool ivm_span_is_empty(ivm_span span) {

  return span.begin == span.end;
}

char ivm_span_get_char(ivm_span span, int index) {
  
  ivm_verify_file(span.file);

  int pos = index >= 0 ? span.begin + index : span.end + index;

  check$(pos >= (int) span.begin, "Character to get must be inside the span");
  check$(pos < (int) span.end, "Character to read must be inside the span");
  check$(pos < (int) span.file->size, "Character must be inside the file");

  return span.file->contents[pos];
}

size_t ivm_span_get_length(ivm_span span) {
  return span.end - span.begin;
}

ivm_span ivm_span_slice(ivm_span span, int begin, int end) {

  int begin_pos = begin == ivm_span_slice_to_end ? span.end :
                       begin >= 0 ? span.begin + begin : span.end + begin;
  int end_pos = end == ivm_span_slice_to_end ? span.end :
                       end >= 0 ? span.begin + end : span.end + end;

  check$((int) span.begin <= begin_pos, "Sliced span should be inside original span");
  check$(begin_pos <= end_pos, "Sliced span shall have its begin before its end");
  check$(end_pos <= (int) span.end, "Sliced span should be inside original span");

  return (ivm_span) {
    .file = span.file,
    .begin = begin_pos,
    .end = end_pos
  };
}

ivm_sloc ivm_span_get_begin(ivm_span span) {
  return (ivm_sloc) { .file=span.file, .offset=span.begin };
}

ivm_sloc ivm_span_get_end(ivm_span span) {
  return (ivm_sloc) { .file=span.file, .offset=span.end };
}

bool ivm_span_equals_to_span(ivm_span a, ivm_span b) {
  ivm_verify_file(a.file);
  ivm_verify_file(b.file);

  if (ivm_span_get_length(a) != ivm_span_get_length(b))
    return false;

  for (size_t i = 0; i < ivm_span_get_length(a); ++i)
    if (ivm_span_get_char(a, i) != ivm_span_get_char(b, i))
      return false;

  return true;
}

char* ivm_span_get_text(ivm_span span) {
  ivm_verify_file(span.file);

  char* buf = calloc(ivm_span_get_length(span)+1, 1);
  if (!buf)
    return NULL;

  memcpy(buf, span.file->contents + span.begin, ivm_span_get_length(span));
  return buf;
}

void ivm_span_fprint(FILE* out, ivm_span span) {
  for (size_t i = 0; i < ivm_span_get_length(span); ++i) {
    fputc(ivm_span_get_char(span, i), out);
    // When something weird happens
    //fprintf(out, ESC_GRAY"<%02x>"ESC_RST, ivm_span_get_char(span, i));
  }
}

bool ivm_span_equals_to_str(ivm_span span, const char* str) {
  for (size_t i = 0; i < ivm_span_get_length(span); ++i)
    if (ivm_span_get_char(span, i) != str[i]) // This includes \0 byte in the string
      return false;
  return str[ivm_span_get_length(span)] == '\0';
}
