#include "ivm/asm/calc.h"
#include "ivm/common/parser/error.h"
#include "ivm/common/parser/span.h"
#include "ivm/common/parser/match.h"
#include "ivm/asm/symtab.h"
#include "ivm/common/macros.h"
#include "ivm/common/array.h"
#include <ctype.h>
#include <setjmp.h>
#include <stdio.h>
#include <sysexits.h>
#include <stdlib.h>

// Here we go again, writting shunting yard algorithms...
// But now with all the symbol table complexity. Yeah, this is not fun.
//
// TODO: maybe remove clang/gcc specific case ranges (`case 0 ... 3: `)
//
// NOTE: I hate all the symbol table stuff, but there are no lambdas in C.
//       Otherwise I would've just returned a lambda from compute_expression,
//       and the code would be much better.
//
//       I want to go to rewrite it in rust....
//       

typedef enum {
  TOK_NAME,
  TOK_NUMBER,
  TOK_OPERATOR,
  TOK_LPAREN,
  TOK_RPAREN
} token_type;

/// \brief Result of evaluation of the expression
typedef struct {
  ivm_span extent;
  ivm_sym_value_t value;
} shyu_res_t;


static const char* get_token_name(token_type tt) {
  switch(tt) {
    case TOK_NAME: return "name";
    case TOK_NUMBER: return "number";
    case TOK_OPERATOR: return "operator";
    case TOK_LPAREN: return "`(`";
    case TOK_RPAREN: return "`)`";
    default: die$("Unknown token type!");
  }
}

//=====[ Matching tokens ]======================================================

/// \brief Check if that character can be a part of the sign
static bool is_sign_char(char c) {
  switch(c) {
    case '-':
    case '+':
    case '/':
    case '*':
    case '%':
    case '<':
    case '>':
    case '=':
    case '&':
    case '|':
    case '~':
    case '^':
      return true;
    default:
      return false;
  }
}

/// \brief Match a token
/// \param span Span to match token from
/// \param [out] typ Where to write type of the extracted token
/// \param [out] tok Where to write the extracted token
/// \param report_error Place to jump to in case of error
/// \returns Leftover part of the given span
static ivm_span match_expr_token(ivm_span span, token_type* typ, ivm_span* tok,
                                 jmp_buf report_error) {

  check$(!ivm_span_is_empty(span), "Cannot match token from an empty span");
  check$(typ, "You should provide a pointer to put token type into");
  check$(tok, "You should provide a place to put matched token into");

  char first_ch = ivm_span_get_char(span, 0);

  if (first_ch == '(' || first_ch == ')') {
    // A parenthesis
    *typ = first_ch == '(' ? TOK_LPAREN : TOK_RPAREN;
    *tok = ivm_span_slice(span, 0, 1);
    return ivm_span_slice(span, 1, ivm_span_slice_to_end);

  } else if (isdigit(first_ch)) {
    // Number. It may be hexamedical, or octal, or binary
    *typ = TOK_NUMBER;
    return ivm_match_number(span, tok);

  } else if (isalpha(first_ch) || first_ch == '_') {
    // Some name
    *typ = TOK_NAME;
    return ivm_match_identifier(span, tok);

  } else if (is_sign_char(first_ch)) {
    // An operator
    size_t i = 1;
    while (i < ivm_span_get_length(span) && is_sign_char(ivm_span_get_char(span, i)))
      ++i;

    *typ = TOK_OPERATOR;
    *tok = ivm_span_slice(span, 0, i);
    return ivm_span_slice(span, i, ivm_span_slice_to_end);

  } else {
    // Something completely unexpected
    ivm_report(REPORT_ERROR, ivm_span_slice(span, 0, 1), "Found an unexpected character in math expression");
    ivm_report_add_note("This is not a parenthesis (like `(` or `)`)");
    ivm_report_add_note("This is also not a number");
    ivm_report_add_note("Also, this cannot be a variable name (those contain lattin letters and `_`)");
    ivm_report_add_note("This character is not the one, occuring in operator names");
    longjmp(report_error, 0);
  }
}


//=====[ Operators ]============================================================





static size_t binary_operator_priority(ivm_span operator, jmp_buf report_error) {
  if (ivm_span_equals_to_str(operator, "*") || ivm_span_equals_to_str(operator, "/") || ivm_span_equals_to_str(operator, "%"))
    return 1;
  else if (ivm_span_equals_to_str(operator, "+") || ivm_span_equals_to_str(operator, "-"))
    return 2;
  else if (ivm_span_equals_to_str(operator, "<<") || ivm_span_equals_to_str(operator, ">>"))
    return 3;
  // reserve 4 and 5 for comparsions, in case they will be needed later
  else if (ivm_span_equals_to_str(operator, "&"))
    return 6;
  else if (ivm_span_equals_to_str(operator, "^"))
    return 7;
  else if (ivm_span_equals_to_str(operator, "|"))
    return 8;
  else {
    ivm_report(REPORT_ERROR, operator, "Unknown binary operator");
    longjmp(report_error, 0);
  }
}


static shyu_res_t compute_binary(ivm_span operator,
                                 shyu_res_t left, shyu_res_t right,
                                 jmp_buf report_error) {
  
  shyu_res_t result = (shyu_res_t) {
    .value = 0,
    .extent = ivm_span_new(
        ivm_span_get_begin(left.extent),
        ivm_span_get_end(right.extent)
    )
  };

  #define COMMON_OPERATOR(op) \
    if (ivm_span_equals_to_str(operator, #op))\
      { result.value = left.value op right.value; }

  // TODO: check for overflows

  COMMON_OPERATOR(*)

  else if (ivm_span_equals_to_str(operator, "/")) {
    if (right.value == 0) {
      ivm_report(REPORT_ERROR, operator, "Division by zero");
      ivm_report(REPORT_NOTE, right.extent, "This evaluates to zero");
      longjmp(report_error, 0);
    }
    result.value = left.value / right.value;
  }

  else if (ivm_span_equals_to_str(operator, "%")) {
    if (right.value == 0) {
      ivm_report(REPORT_ERROR, operator, "Modulo taken by zero");
      ivm_report(REPORT_NOTE, right.extent, "This evaluates to zero");
      longjmp(report_error, 0);
    }
    result.value = left.value % right.value;
  }

  else COMMON_OPERATOR(+) 
  else COMMON_OPERATOR(-) 
  else COMMON_OPERATOR(<<)
  else COMMON_OPERATOR(>>)
  else COMMON_OPERATOR(&)
  else COMMON_OPERATOR(^)
  else COMMON_OPERATOR(|)
  else die$("Invalid operators were supposed to be caught during `binary_operator_priority`???");

  #undef COMMON_OPERATOR

  return result;
}


static shyu_res_t compute_unary(ivm_span operator, shyu_res_t param, jmp_buf report_error) {

  shyu_res_t result = (shyu_res_t) {
    .value = 0,
    .extent = ivm_span_new(
        ivm_span_get_begin(operator),
        ivm_span_get_end(param.extent)
    )
  };

  if (ivm_span_equals_to_str(operator, "-"))
    result.value = -param.value;
  else if (ivm_span_equals_to_str(operator, "~"))
    result.value = ~param.value;
  else {
    ivm_report(REPORT_ERROR, operator, "Unknown unary operator");
    longjmp(report_error, 0);
  }

  return result;
}


//=====[ Parsing numbers ]======================================================


static ivm_sym_value_t parse_digit(ivm_sym_value_t base, char digit) {

  check$(base == 2 || base == 8 || base == 10 || base == 16, "Only those bases are supported");

  switch(digit) {
    
    // Hex "digits"
    case 'A' ... 'F':
      if (base != 16)
        die$("Hexamedical character in base %d number", base);
      return digit - 'A' + 10;
    
    case 'a' ... 'f':
      if (base != 16)
        die$("Hexamedical character in base %d number", base);
      return digit - 'a' + 10;

    case '9':
    case '8':
      if (base == 8)
        die$("Character %c (%02x) in octal number", digit, digit);

    case '2' ... '7':
      if (base == 2)
        die$("Character %c (%02x) in binary number", digit, digit);

    case '1':
    case '0':
      return digit - '0';

    default:
      die$("Unknown character %c (%02x) in number", digit, digit);
  }
}

/// \brief Parse a number read by `ivm_match_number`
///
/// Note what this thing will `die$()` if it sees invalid characters!
/// Also it will report errors if it overflows
///
/// \param num The number to parse
/// \returns The parsed number
static ivm_sym_value_t parse_number(ivm_span num, jmp_buf report_errors) {

  ivm_span untrimmed_num = num;

  ivm_sym_value_t base = 10;

  // Maybe read a base specifier, like `0x`
  if (ivm_span_get_length(num) >= 2 && ivm_span_get_char(num, 0) == '0') {
    char bc = ivm_span_get_char(num, 1);
    switch(bc) {
      case 'b': base = 2;  break;
      case 'o': base = 8;  break;
      case 'x': base = 16; break;
      case '0' ... '9': break; 
      default: die$("Unexpected character `%c` (%02x) in number", bc, bc);
    }
    if (base != 10) // Trim the prefix
      num = ivm_span_slice(num, 2, ivm_span_slice_to_end);
  }

  ivm_sym_value_t val = 0;

  for (size_t i = 0; i < ivm_span_get_length(num); ++i) {
    if (ivm_span_get_char(num, i) == '.') {
      ivm_report(REPORT_ERROR, untrimmed_num, "Floating-point numbers are not supported yet. Sorry.");
      longjmp(report_errors, 0);
    }
    // FIXME: add overflow checking
    val = val * base + parse_digit(base, ivm_span_get_char(num, i));
  }

  return val;
}

//=====[ The shunting yard itself ]=============================================

typedef enum {
  SHYU_OP_BINARY,
  SHYU_OP_UNARY,
  SHYU_LBRACE
} shyu_operator_type;

typedef struct {
  shyu_operator_type op_type;
  ivm_span operator;
} shyu_operator;

static void execute_operator(shyu_operator op, ia_arr$(shyu_res_t) *stk, jmp_buf report_error) {

  shyu_res_t a, b, res;

  switch(op.op_type) {

    case SHYU_OP_BINARY:
      check$(ia_length(*stk) >= 2, "Binary operator requires at least two items on the stack, but we have %zu", ia_length(*stk));
      b = ia_top$(*stk);
      ia_pop$(stk);
      a = ia_top$(*stk);
      ia_pop$(stk);
      res = compute_binary(op.operator, a, b, report_error);
      ia_push$(stk, res);
      return;

    case SHYU_OP_UNARY:
      check$(ia_length(*stk) >= 1, "Unary operator requires an item on the stack");
      a = ia_top$(*stk);
      ia_pop$(stk);
      res = compute_unary(op.operator, a, report_error);
      ia_push$(stk, res);
      return;

    case SHYU_LBRACE:
      die$("execute_operator() is meant to execute operators, but instead it got `(`");

    default:
      die$("A thing of unknown type was found on the operator stack");
  }
}

typedef struct {
  const char* description;
  ivm_span ref;
  ivm_span expr;
  bool is_anon;
} _expr_symtab_entry;

/// \brief This function actually computes the expression
static ivm_sym_value_t _compute_fn(ivm_symtab_entry* entry, ivm_symtab symtab,
                                  void* state,
                                  ivm_sym_value_t (*get_value)(void* state, ivm_symtab_entry* entry)) {

  check$(entry->param, "Data used by compute_fn must be present");
  _expr_symtab_entry* param = (_expr_symtab_entry*) entry->param;

  // Now do the calculation 
  
  ia_arr$(shyu_operator) op_stack = ia_new_empty_array$(shyu_operator);
  ia_arr$(shyu_res_t) res_stack = ia_new_empty_array$(shyu_res_t);

  jmp_buf report_error;

  if (setjmp(report_error)) {
    // OK, an error was reported
    // Print where we are
    ivm_report(REPORT_NOTE, param->ref, "This occured during computation of this %s", param->description);
    exit(EX_DATAERR);
  }

  ivm_span expr = param->expr;

  expr = ivm_trim_ws_and_comments(expr);
  if (ivm_span_is_empty(expr)) {
    // Expression is empty, except comments and whitespaces
    ivm_report(REPORT_ERROR, (ivm_span) {
        .file = param->expr.file,
        .begin = param->expr.begin,
        .end = param->expr.begin
      }, "Expected an expression here");
    exit(EX_DATAERR);
  }

  // HACK: This starting value removes the need for special starting token type.
  //
  //       This works because for token sequence checker it looks like the
  //       expression starts with `(`, which is like wrapping all the expression
  //       in `()`, which will work for any normal expression.
  //
  token_type prev_ttype = TOK_LPAREN; 
  ivm_span prev_tok = ivm_span_slice(expr, 0, 0);

  while (!ivm_span_is_empty(expr)) {

    expr = ivm_trim_ws_and_comments(expr);
    if (ivm_span_is_empty(expr))
      break;

    ivm_span tok;
    token_type ttype;
    expr = match_expr_token(expr, &ttype, &tok, report_error);

    switch (ttype) {

      case TOK_NUMBER:
        if (prev_ttype == TOK_NUMBER || prev_ttype == TOK_NAME ||
            prev_ttype == TOK_RPAREN) {
          // Something like `name 123`. 
          ivm_report(REPORT_ERROR, tok, "Expected an operator between %s and this (%s)",
                     get_token_name(prev_ttype), get_token_name(ttype));
          longjmp(report_error, 0);
        }

        ivm_sym_value_t num_value = parse_number(tok, report_error);
        ia_push$(&res_stack, ((shyu_res_t) {
          .value = num_value,
          .extent = tok
        }));
        break;

      case TOK_NAME:
        if (prev_ttype == TOK_NUMBER || prev_ttype == TOK_NAME ||
            prev_ttype == TOK_RPAREN) {
          // Something like `name 123`. 
          ivm_report(REPORT_ERROR, tok, "Expected an operator between %s and this (%s)",
                     get_token_name(prev_ttype), get_token_name(ttype));
          longjmp(report_error, 0);
        }

        ivm_symtab_entry* ent = ivm_symtab_get_entry(symtab, tok);
        if (!ent) {
          ivm_report(REPORT_ERROR, tok, "This name is not known");
          longjmp(report_error, 0);
        }

        ivm_sym_value_t name_value = get_value(state, ent);
        ia_push$(&res_stack, ((shyu_res_t){
          .value = name_value,
          .extent = tok
        }));
        break;

      case TOK_OPERATOR:
        bool is_binary = prev_ttype == TOK_NUMBER
                      || prev_ttype == TOK_NAME
                      || prev_ttype == TOK_RPAREN;
        
        size_t op_prec = binary_operator_priority(tok, report_error);
        while (is_binary
              && ia_length(op_stack)
              && ia_top$(op_stack).op_type != SHYU_LBRACE
              && binary_operator_priority(ia_top$(op_stack).operator, report_error) >= op_prec) {
          execute_operator(ia_top$(op_stack), &res_stack, report_error);
          ia_pop$(&op_stack);
        }

        ia_push$(&op_stack, ((shyu_operator) {
          .operator = tok,
          .op_type = is_binary ? SHYU_OP_BINARY : SHYU_OP_UNARY
        }));
        break;

      case TOK_LPAREN:
        ia_push$(&op_stack, ((shyu_operator) {
          .operator = tok,
          .op_type = SHYU_LBRACE
        }));
        break;

      case TOK_RPAREN:
        while (ia_length(op_stack) && ia_top$(op_stack).op_type != SHYU_LBRACE)
          execute_operator(ia_top$(op_stack), &res_stack, report_error);
        if (!ia_length(op_stack)) {
          ivm_report(REPORT_ERROR, tok, "Mismatched `)`");
          longjmp(report_error, 0);
        }
        ia_pop$(&op_stack);
        break;

      default:
        die$("If you have added a new type of token, add it into switch case in `ivm_compute_expression()`");
    }

    prev_ttype = ttype;
    prev_tok = tok;
  }

  while (ia_length(op_stack)) {
    shyu_operator op = ia_top$(op_stack);
    if (op.op_type == SHYU_LBRACE) {
      ivm_report(REPORT_ERROR, op.operator, "Mismatched `)`");
      longjmp(report_error, 0);
    }
    execute_operator(op, &res_stack, report_error);
    ia_pop$(&op_stack);
  }

  check$(ia_length(res_stack) == 1, "There should be one value left after execution of the expression");

  return res_stack[0].value;
}


static void _print_name(ivm_symtab_entry* entry) {
  check$(entry->param, "Data used by compute_fn must be present");
  _expr_symtab_entry* param = (_expr_symtab_entry*) entry->param;

  fprintf(stderr, ESC_GREEN "%s" ESC_RST, param->description);
  if (!param->is_anon) {
    fputc(' ', stderr);
    ivm_span_fprint(stderr, param->ref);
  }
}

static ivm_sym_value_t _fake_get_value(void* state, ivm_symtab_entry* ent) {
  (void) state;

  check$(ent, "Entry to get value of must be present");
  check$(ent->sym_compute_step == SYM_COMPUTE_DONE, "Symbol table values must be computed");

  return ent->value;
}

ivm_sym_value_t ivm_compute_expression(ivm_span ref, const char* descr,
                                       ivm_span expr, ivm_symtab* symbol_table) {

  _expr_symtab_entry param = (_expr_symtab_entry) {
    .expr = expr,
    .ref = ref,
    .description = descr
  };

  ivm_symtab_entry ent = (ivm_symtab_entry) {
    // Everything else is not used
    .param = &param
  };

  return _compute_fn(&ent, *symbol_table, symbol_table, _fake_get_value);
}

ivm_symtab_entry* ivm_add_computed_entry(ivm_span ref, const char* descr, ivm_span expr, ivm_symtab* symbol_table, bool is_anonymous) {

  _expr_symtab_entry* param = (_expr_symtab_entry*) calloc(1, sizeof(_expr_symtab_entry));
  param->expr = expr;
  param->description = descr;
  param->ref = ref;
  param->is_anon = false;

  return ivm_symtab_add_entry(symbol_table, (ivm_symtab_entry) {
    .param = param,
    .name = is_anonymous ? (ivm_span) { .file=NULL, .begin=0, .end=0 } : ref,
    .print_name = &_print_name,
    .compute_fn = &_compute_fn,
    .destroy_param = &free,
    .sym_compute_step = SYM_COMPUTE_NOT_DONE
  });
}
