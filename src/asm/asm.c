#include "ivm/asm/asm.h"
#include "ivm/asm/calc.h"
#include "ivm/asm/ast.h"
#include "ivm/common/parser/error.h"
#include "ivm/common/parser/span.h"
#include "ivm/asm/symtab.h"
#include "ivm/common/array.h"
#include "ivm/common/macros.h"

#include <stdint.h>
#include <ctype.h>
#include <endian.h>
#include <stdlib.h>
#include <sysexits.h>

// TODO: sections

//=====[ Labels ]===============================================================

typedef struct {
  // Byte size of all non-variable items before this label
  size_t location;
} _label_param;

static ivm_sym_value_t _compute_label(
      ivm_symtab_entry* entry, ivm_symtab symtab,
      void* state,
      ivm_sym_value_t (*get_value)(void* state, ivm_symtab_entry* entry)
  ) {
  (void) symtab;
  (void) state;

  check$(entry, "Entry must be present to compute its value");
  check$(entry->param, "Label parameter must exist");
  check$(get_value, "Function to get value must exist");

  _label_param* param = (_label_param*) entry->param;
  return param->location;
}

static void _print_label(ivm_symtab_entry* entry) {

  check$(entry, "Entry to print must exist");
  ivm_verify_file(entry->name.file);

  fputs(ESC_GREEN "label " ESC_RST, stderr);
  ivm_span_fprint(stderr, entry->name);
}

// Get number of bytes emitted by number directive
static size_t num_dir_sz(ivm_span dir) {
  if (ivm_span_equals_to_str(dir, ".u8") || ivm_span_equals_to_str(dir, ".i8"))
    return 1;
  if (ivm_span_equals_to_str(dir, ".u16") || ivm_span_equals_to_str(dir, ".i16"))
    return 2;
  if (ivm_span_equals_to_str(dir, ".u32") || ivm_span_equals_to_str(dir, ".i32"))
    return 4;
  if (ivm_span_equals_to_str(dir, ".u64") || ivm_span_equals_to_str(dir, ".i64"))
    return 8;
  die$("Unknown number directive");
}

static int hexdigit2int(char c) {
  if (c >= 'a') return c - 'a' + 10;
  else if (c >= 'A') return c - 'A' + 10;
  else return c - '0';
}

/// Read one character from string (something quoted)
/// written in the span.
/// This handles the escapes and such.
static ivm_span read_one_character(ivm_span str, char* c) {

  check$(c, "Should get a place to put read character into");
  check$(!ivm_span_is_empty(str), "Should get something to read, but got an empty span");

  if (ivm_span_get_char(str, 0) != '\\') {
    // Something not escaped
    *c = ivm_span_get_char(str, 0);
    return ivm_span_slice(str, 1, ivm_span_slice_to_end);
  }

  if (ivm_span_get_length(str) == 1) {
    ivm_report(REPORT_ERROR, ivm_span_slice(str, 0, 1), "Expected something escaped after this");
    ivm_report_add_note("Please tell me how you managed to achive this");
    exit(EX_DATAERR);
  }

  char escaped = ivm_span_get_char(str, 1);

  #define NORMAL_ESC(ch, out) case ch:\
                            *c = out; goto normal_escape_seq;

  switch (escaped) {

    // Normal escape sequences
    NORMAL_ESC('b', '\b')
    NORMAL_ESC('f', '\f')
    NORMAL_ESC('n', '\n')
    NORMAL_ESC('r', '\r')
    NORMAL_ESC('t', '\t')
    NORMAL_ESC('\\', '\\')
    NORMAL_ESC('0', '\0')
    NORMAL_ESC('"', '"')

    // Hex code
    case 'x': {
      if (ivm_span_get_length(str) < 4) {
        ivm_report(REPORT_ERROR, ivm_span_slice(str, 0, 2), "Expected two hexamedical digits after `\\x`");
        ivm_report_add_note("`\\x` is used to specify characters in hex, for example `\\x41` is an `A`.");
        exit(EX_DATAERR);
      }

      char a = ivm_span_get_char(str, 2), b = ivm_span_get_char(str, 3);
      if (!isxdigit(a) || !isxdigit(b)) {
        ivm_report(REPORT_ERROR, ivm_span_slice(str, 0, 4), "Non-hex digits occured in hex escape code");
        ivm_report_add_note("`\\x` is used to specify characters in hex, for example `\\x41` is an `A`.");
        exit(EX_DATAERR);
      }

      *c = hexdigit2int(a) * 16 + hexdigit2int(b);

      return ivm_span_slice(str, 4, ivm_span_slice_to_end);
    }
  }

  #undef NORMAL_ESC

 normal_escape_seq:
  return ivm_span_slice(str, 2, ivm_span_slice_to_end);
}

static size_t string_token_length(ivm_span str) {
  size_t res = 0;
  str = ivm_span_slice(str, 1, -1);
  char _;

  while (!ivm_span_is_empty(str)) {
    str = read_one_character(str, &_);
    ++res;
  }

  return res;
}

void iasm_emit_number(FILE* output, ivm_sym_value_t val, size_t num_bytes, bool no_sign, ivm_span ref) {

  #define VARIANT(size, has_no_sign, type, conv, min, max) \
    if (num_bytes == (size) && no_sign == (has_no_sign)) { \
      if (val > (long long) max) {\
        ivm_report(REPORT_WARNING, ref, \
                   "Value %lld will overflow %s %zu-byte integer (its max value is %lld)",\
                   val, (no_sign ? "unsigned" : "signed"), num_bytes, (long long) (max)); \
      } \
      if (val < (long long) min) {\
        ivm_report(REPORT_WARNING, ref, \
                   "Value %lld will underflow %s %zu-byte integer (its min value is %lld)",\
                   val, (no_sign ? "unsigned" : "signed"), num_bytes, (long long) (min)); \
      } \
      type le_value = conv((type) val); \
      fwrite(&le_value, num_bytes, 1, output); \
      return; \
    }

  VARIANT(1, true, uint8_t, +, 0, UINT8_MAX)
  VARIANT(1, false, int8_t, +, INT8_MIN, INT8_MAX) 
  VARIANT(2, true, uint16_t, htole16, 0, UINT16_MAX)
  VARIANT(2, false, int16_t, htole16, INT16_MIN, INT16_MAX) 
  VARIANT(4, true, uint32_t, htole32, 0, UINT32_MAX)
  VARIANT(4, false, int32_t, htole32, INT32_MIN, INT32_MAX) 
  // FIXME: use bigger type for symbol values
  //VARIANT(8, true, uint64_t, htole64, 0, UINT64_MAX)
  VARIANT(8, true, int64_t, htole64, INT64_MIN, INT64_MAX) // For now this is here
  VARIANT(8, false, int64_t, htole64, INT64_MIN, INT64_MAX) 

  die$("Unknown size and signness: %s %zu-byte number", (no_sign ? "unsigned" : "signed"), num_bytes);

  #undef VARIANT
}


void iasm_assemble(FILE* output,
                  ia_arr$(ast_node) ast,
                  iasm_fn_to_get_instr_length get_len,
                  iasm_fn_to_emit_instr get_bytecode) {

  // First pass : labels, constants, and offset computation

  // List of variable-sized elements
  ia_arr$(ivm_symtab_entry*) variable_elements = ia_new_empty_array$(ivm_symtab_entry*);
  ivm_symtab symtab = ivm_symtab_new();

  // Will be non-const later with support for the sections.
  const size_t label_offset = 0x10000;

  size_t location = label_offset;


  for (size_t i = 0; i < ia_length(ast); ++i) {
    switch(ast[i].type) {

      case AST_LABEL: {
        
        _label_param* lp = (_label_param*) calloc(1, sizeof(_label_param));
        check$(lp, "Memory should be allocated");
        lp->location = location;

        ivm_symtab_add_entry(&symtab, (ivm_symtab_entry) {
          .name = ast[i].as_label.name,
          .param = lp,
          .print_name = _print_label,
          .compute_fn = _compute_label,
          .destroy_param = free,
        });
        break;
      }

      case AST_INSTR: {
        location += get_len(ast[i].as_instr.name, ast[i].as_instr.args);
        break;
      }

      case AST_DIR_CONST: {
        ivm_add_computed_entry(ast[i].as_dir_const.name, "constant", ast[i].as_dir_const.expression, &symtab, false);
        break;
      }

      case AST_DIR_BLOB: {
        if (ivm_span_equals_to_str(ast[i].as_dir_blob.dir_name, ".base64")) {
          ivm_report(REPORT_ERROR, ast[i].as_dir_blob.dir_name, "Sorry, but I will add support for this later");
          exit(EX_DATAERR);
        }
        location += string_token_length(ast[i].as_dir_blob.value);
        break;
      }

      case AST_DIR_NUMBER: {
        location += num_dir_sz(ast[i].as_dir_number.dir_name);
        break;
      }

      default:
        die$("Unknown AST item type!");
    }
  }  

  ivm_symtab_compute_values(symtab);

  // Second pass: generate the thing
  
  for (size_t i = 0; i < ia_length(ast); ++i) {
    
    switch(ast[i].type) {
      case AST_LABEL:
      case AST_DIR_CONST:
        continue;

      case AST_INSTR:
        get_bytecode(ast[i].as_instr.name, ast[i].as_instr.args, &symtab, output);
        break;

      case AST_DIR_BLOB: {
        // No base64 for now
        ivm_span string_contents = ivm_span_slice(ast[i].as_dir_blob.value, 1, -1);
        while (!ivm_span_is_empty(string_contents)) {
          char c;
          string_contents = read_one_character(string_contents, &c);
          fputc(c, output);
        }
        break;
      }

      case AST_DIR_NUMBER: {
        bool is_unsigned = ivm_span_get_char(ast[i].as_dir_number.dir_name, 1) == 'u';
        size_t num_bytes = num_dir_sz(ast[i].as_dir_number.dir_name);
        ivm_sym_value_t val = ivm_compute_expression(
            ast[i].as_dir_number.dir_name, "number directive",
            ast[i].as_dir_number.number_expr, &symtab
        );
        iasm_emit_number(output, val, num_bytes, is_unsigned, ast[i].as_dir_number.number_expr);
        break;
      }
    }
  }
}


