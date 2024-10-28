#include "ivm/asm/ast.h"
#include "ivm/common/macros.h"
#include "ivm/common/parser/error.h"
#include "ivm/common/parser/match.h"
#include "ivm/common/parser/span.h"
#include "ivm/common/array.h"

#include <sysexits.h>
#include <stdlib.h>

static ivm_span trim_end_comment(ivm_span line) {
  size_t end = 0;
  while (end+1 < ivm_span_get_length(line) &&
         !(ivm_span_get_char(line, end) == '/' && ivm_span_get_char(line, end+1) == '/'))
    ++end;
  if (end+1 == ivm_span_get_length(line))
    ++end;
  return ivm_span_slice(line, 0, end);
}

static ivm_span parse_directive(ivm_span span, ivm_span dir_name, ast_node* out) {

  if (ivm_span_equals_to_str(dir_name, ".const")) {
    // Constant declaration
    span = ivm_trim_ws(span);

    ivm_span name;
    span = ivm_match_identifier(span, &name);
    if (ivm_span_is_empty(name)) {
      ivm_report(REPORT_ERROR, name, "Expected a constant name here");
      exit(EX_DATAERR);
    }
    span = ivm_trim_ws(span);

    out->type = AST_DIR_CONST;
    out->as_dir_const.dir_name = dir_name;
    out->as_dir_const.name = name;
    out->as_dir_const.expression = span;

    return ivm_span_slice(span, ivm_span_slice_to_end, ivm_span_slice_to_end);
  }

  else if (ivm_span_equals_to_str(dir_name, ".ascii") || ivm_span_equals_to_str(dir_name, ".base64")) {
    // Some blob
    span = ivm_trim_ws(span);
    ivm_span str;
    span = ivm_match_string(span, &str);



    span = ivm_trim_ws_and_comments(span);

    if (!ivm_span_is_empty(span)) {
      ivm_report(REPORT_ERROR, span, "Extra characters after blob directive");
      ivm_report_add_note("Unlike real, big assemblers this directive takes only ONE string as argument");
      ivm_report_add_note("So, you cannot do `.ascii \"Foo\" \"bar\"` here");
      exit(EX_DATAERR);
    }

    out->type = AST_DIR_BLOB;
    out->as_dir_blob.value = str;
    out->as_dir_blob.dir_name = dir_name;

    return span;
  }

  else if (ivm_span_get_length(dir_name) >= 3 && 
           (ivm_span_get_char(dir_name, 1) == 'i' || ivm_span_get_char(dir_name, 1) == 'u') &&
           (ivm_span_equals_to_str(ivm_span_slice(dir_name, 2, ivm_span_slice_to_end), "8") ||
            ivm_span_equals_to_str(ivm_span_slice(dir_name, 2, ivm_span_slice_to_end), "16") ||
      ivm_span_equals_to_str(ivm_span_slice(dir_name, 2, ivm_span_slice_to_end), "32") ||
            ivm_span_equals_to_str(ivm_span_slice(dir_name, 2, ivm_span_slice_to_end), "64"))) {
    // Number directive
    
    if (ivm_span_equals_to_str(dir_name, ".u64")) {
      ivm_report(REPORT_WARNING, dir_name,
                 "`.u64` directive is useless for now because all "
                 "numbers are represented as `long long` during assembly");
    }

    span = ivm_trim_ws(span);

    out->type = AST_DIR_NUMBER;
    out->as_dir_number.number_expr = span;
    out->as_dir_number.dir_name = dir_name;
    return ivm_span_slice(span, ivm_span_slice_to_end, ivm_span_slice_to_end);
  }

  else {
    ivm_report(REPORT_ERROR, dir_name, "Unknown directive");
    exit(EX_DATAERR);
  }
}

static ivm_span parse_one(ivm_span line, ast_node* out) {

  if (ivm_span_get_char(line, 0) == '.') {
    // A directive!
    ivm_span ident;
    ivm_match_identifier(ivm_span_slice(line, 1, ivm_span_slice_to_end), &ident);
    
    // We trimmed `.` before reading, get it back.
    ident.begin = line.begin;

    // Remove name from the span
    line = ivm_span_slice(line, ivm_span_get_length(ident), ivm_span_slice_to_end);

    ivm_span arg;
    line = ivm_match_line(line, &arg);
    arg = parse_directive(arg, ident, out);
    arg = ivm_trim_ws_and_comments(arg);

    if (!ivm_span_is_empty(arg)) {
      ivm_report(REPORT_ERROR, arg, "Extra characters after directive argument");
      exit(EX_DATAERR);
    }

    return line;
  }

  ivm_span name;
  line = ivm_match_identifier(line, &name);

  if (ivm_span_is_empty(name)) {
    // Failed to read a label/command name, and this is not a directive
    ivm_report(REPORT_ERROR, ivm_span_slice(line, 0, 1), "Unknown character found");
    ivm_report_add_note("Expected a command, label or directive here");
    ivm_report_add_note("Labels and commands start with C identifiers, but this does not");
    ivm_report_add_note("Directives start with `.`, this does not");
    exit(EX_DATAERR);
  }

  line = ivm_trim_ws(line);
  
  if (!ivm_span_is_empty(line) && ivm_span_get_char(line, 0) == ':') {
    // A label!
    out->type = AST_LABEL;
    out->as_label.name = name;
    return ivm_span_slice(line, 1, ivm_span_slice_to_end); // remove `:`
  } else {
    // A instruction
    out->type = AST_INSTR;
    out->as_instr.name = name;
    out->as_instr.args = line;
    return ivm_span_slice(line, ivm_span_slice_to_end, ivm_span_slice_to_end);
  }

  return line;
}

ia_arr$(ast_node) iasm_parse_file(ivm_file* file) {

  ivm_span span = ivm_file_full_span(file);
  ia_arr$(ast_node) res = ia_new_empty_array$(ast_node);

  while (!ivm_span_is_empty(span)) {
    span = ivm_trim_ws_and_comments(span);
    if (ivm_span_is_empty(span))
      break;
    ivm_span line;
    span = ivm_match_line(span, &line);
    line = trim_end_comment(line);


    if (ivm_span_is_empty(line))
      continue;

    ast_node node;
    line = parse_one(line, &node);
    line = ivm_trim_ws(line);

    if (node.type != AST_LABEL && !ivm_span_is_empty(line)) {
      // Something left on the line
      // Labels can have that (they can prefix stuff)
      ivm_report(REPORT_ERROR, ivm_span_slice(line, 0, 0), "Unexpected stuff left on the line");
      ivm_report_add_note("If you encountered this, please contact me. This should never happen.");
      exit(EX_DATAERR);
    }

    // If something was left after label, we want it
    // back in the span.
    // (remember, we cut off current line from it to match on it)
    span.begin = line.begin;

    ia_push$(&res, node);
  }

  return res;
}

void iasm_dump_ast(ia_arr$(ast_node) ast) {

  fputs(ESC_GRAY "\n// AST dump\n"
                 "// Original comments, blank lines and spacing are removed\n"
                 ESC_RST, stdout);
  // Just print the file nicely
  for (size_t i = 0; i < ia_length(ast); ++i) {
    
    switch(ast[i].type) {

      case AST_LABEL:
        // Who was that genius who decided what `puts` shall
        // print a trailing newline?
        fputs(ESC_GREEN, stdout);
        ivm_span_fprint(stdout, ast[i].as_label.name);
        fputs(ESC_RST ESC_GRAY ":", stdout);
        break;

      case AST_INSTR:
        fputs(ESC_BLUE "  ", stdout);
        ivm_span_fprint(stdout, ast[i].as_instr.name);
        fputs(ESC_RST " " ESC_YELLOW, stdout);
        ivm_span_fprint(stdout, ast[i].as_instr.args);
        break;

      case AST_DIR_CONST:
        fputs(ESC_RED, stdout);
        ivm_span_fprint(stdout, ast[i].as_dir_const.dir_name);
        fputs(ESC_RST " " ESC_BLUE, stdout);
        ivm_span_fprint(stdout, ast[i].as_dir_const.name);
        fputs(ESC_RST " " ESC_YELLOW, stdout);
        ivm_span_fprint(stdout, ast[i].as_dir_const.expression);
        break;

      case AST_DIR_NUMBER:
        fputs(ESC_RED, stdout);
        ivm_span_fprint(stdout, ast[i].as_dir_number.dir_name);
        fputs(ESC_RST " " ESC_YELLOW, stdout);
        ivm_span_fprint(stdout, ast[i].as_dir_number.number_expr);
        break;

      case AST_DIR_BLOB:
        fputs(ESC_RED, stdout);
        ivm_span_fprint(stdout, ast[i].as_dir_blob.dir_name);
        fputs(ESC_RST " " ESC_GREEN, stdout);
        ivm_span_fprint(stdout, ast[i].as_dir_blob.value);
        break;       
    }

    fputs(ESC_RST "\n", stdout);
  }
  fputs("\n", stdout);
}
