/// \file
/// \brief Here the `span` structure is defined
///
///

#ifndef IVM_ASM_PARSER_SPAN
#define IVM_ASM_PARSER_SPAN

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

//=====[ Data structures ]======================================================


/// The file with source code
typedef struct {
  
  /// Name of the file, used in error reporting
  char* name;

  /// Contents of the file
  char* contents;

  /// Total count of characters in the file
  size_t size;

  /// Number of source code lines
  size_t num_lines;

  /// Dynamic array of offsets of line starts,
  /// `num_lines` in length.
  size_t* line_start_offsets;

} ivm_file;


/// A location withn the source code
typedef struct {

  /// Offset in the file
  size_t offset;

  /// The file itself
  ivm_file* file;

} ivm_sloc;


/// Span of code from the source file
typedef struct {

  /// Offset of the first character 
  size_t begin;

  /// Offset of character after the last
  size_t end;

  /// The file itself
  ivm_file* file;

} ivm_span;


//=====[ Methods for files ]====================================================


/// \brief Create a new source code file with given name and contents
/// \param name File name, can be anything (it is used only for error messages)
/// \param contents File contents to be parsed
/// \returns New structure or `NULL` if your computer ran out of memory
ivm_file* ivm_file_new(const char* name, const char* contents);


/// \brief Create a new source code file from file on disk
/// \param filename Path to the file to open
/// \returns New file structure or `NULL` if there is no memory left
ivm_file* ivm_file_open(const char* filename);


/// \brief Destroy source code structure
/// \param file Structure you no longer need
void ivm_file_destroy(ivm_file* file);


/// \brief Check what given file is valid
/// \param file File to check
void ivm_verify_file(ivm_file* file);


/// \brief Get span covering all file contents
/// \param file File to use
/// \returns Span with all the file's contents
ivm_span ivm_file_full_span(ivm_file* file);


//=====[ Methods for source locations ]=========================================


/// \brief Get line of the given location
/// \param location Location in the file
/// \returns Line number, starting from zero
size_t ivm_sloc_get_line(ivm_sloc location);


/// \brief Get column of the location
/// \param location Location in the file
/// \returns Column number, starting from zero
size_t ivm_sloc_get_column(ivm_sloc location);


//=====[ Methods for spans ]====================================================


/// \brief Create new span, spanning between two given locations
/// \param begin Location pointing to the first symbol of the span
/// \param end Location directly after the span
/// \returns The span you asked for
ivm_span ivm_span_new(ivm_sloc begin, ivm_sloc end);


/// \brief Check if span has some characters in it
/// \param span Span to check
/// \returns `true` if span is empty (`begin == end`),
///          `false` otherwise.
bool ivm_span_is_empty(ivm_span span);


/// \brief Get character at given index in the span
///
/// Negative indexing is supported. If you go outside the
/// span, panic will happen.
///
/// \param span Span to extract characters from
/// \param index Index of the character to look at
/// \returns Character at given index inside the span
char ivm_span_get_char(ivm_span span, int index);


/// \brief Get number of characters in the span
/// \param span Span to measure
/// \returns Number of characters in the span
size_t ivm_span_get_length(ivm_span span);


/// \brief Flag to pass to \ref{ivm_span_slice} to
///        slice to the end of the span
static const int ivm_span_slice_to_end = INT_MAX;


/// \brief Slice the span, returning a new span
///
/// This operation is cheap. Also note what indexes can be
/// negative.
///
/// \param span Span to slice
/// \param begin Start of the slice relative to start
///              (or end of the span)
/// \param end End of the slice. If you wish to slice till
///            end, put \ref{ivm_span_slice_to_end} here.
/// \returns Reulting span
ivm_span ivm_span_slice(ivm_span span, int begin, int end);


/// \brief Get the location of beginning of the span
/// \param span Span to get start of
/// \returns Source code location structure
ivm_sloc ivm_span_get_begin(ivm_span span);


/// \brief Get the location of the end of the span
/// \param span The span to get end of
/// \returns Source code location structure, corresponding
///          to the character after the end of the span
ivm_sloc ivm_span_get_end(ivm_span span);


/// \brief Compare text of two spans
bool ivm_span_equals_to_span(ivm_span a, ivm_span b);


/// \brief Allocates a new buffer and stores text of the span there.
/// Freeing is up to you.
char* ivm_span_get_text(ivm_span span);

/// \brief Print text of the span into given file
void ivm_span_fprint(FILE* out, ivm_span span);

/// \brief Compare span to constant string
bool ivm_span_equals_to_str(ivm_span span, const char* str);

#endif
