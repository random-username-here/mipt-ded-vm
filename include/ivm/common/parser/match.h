/// \brief Functions to match some text from the spans
///
/// Here regex are used, because scanf specifiers cannot do `[A-Za-z_][A-Za-z_0-9]+`.
///


#ifndef IVM_ASM_PARSER_MATCH
#define IVM_ASM_PARSER_MATCH

#include "ivm/common/parser/span.h"
#include <regex.h>


/// \brief Return slice of given span with leading whitespaces removed
///
/// This counts everything, which is `isspace()` as whitespaces, including
/// newlines.
///
/// \param span Span with possible leading whitespaces
/// \returns Slice of that span without the leading whitespaces.
ivm_span ivm_trim_ws(ivm_span span);


/// \brief Match comment starting at the start of the span
///
/// Leading whitespaces are not allowed.
///
/// If nothing was matched, `out` is set to empty token, and
/// `span` is returned.
///
/// \param span The span, in which the comment lies
/// \param [out] out The function will put token with the comment text there
/// \returns Slice of all text after the comment
ivm_span ivm_match_comment(ivm_span span, ivm_span* out);


/// \brief Skip all leading whitespaces and comments
///
/// \param span Span, in which to skip spaces and comments
/// \returns That span without leading comments and spaces
ivm_span ivm_trim_ws_and_comments(ivm_span span);


/// \brief Match given regex in the string
///
/// To properly match something, you **must** use
/// beginning-of-line operator (`^`). Otherwise this will
/// match anything withn the span, possibly giving you a
/// match in the middle of the span, which is not what you
/// usually want.
///
/// \param span Span to match regex in
/// \param regex Regex to match. Use `^`!
/// \param [out] out Pointer to location you wish to output span with the matched thing.
ivm_span ivm_match_regex(ivm_span span, const regex_t* regex, ivm_span* out);


/// \brief Match C-style identifier 
///
/// If there was no match, `out` is set to empty token at the start of the `span`.
///
/// \param span Span, at the start of which we expect the identifier
/// \param [out] out Pointer to where to put identifier. Can be `NULL`.
ivm_span ivm_match_identifier(ivm_span span, ivm_span* out);


/// \brief Match a number
///
/// Signs are not allowed here, deal with them yourself.
///
/// Number can be:
///  - A normal number (`123`), which can be floating-point
///  - A hexamedical value (`0xc0ffee` or `0xC0FFEE`)
///  - An octal number (`0o736`)
///  - A binary number (`0b1010011`)
///
/// \param span Span to match the number from
/// \param [out] out Place to put span, containing the number into
/// \returns Leftover
ivm_span ivm_match_number(ivm_span span, ivm_span* out);


/// \brief Compile regex, panicking on compilation errors
void ivm_compile_regex_or_die(regex_t* out, const char* pattern, int flags);


/// \brief Match a string with escape codes
ivm_span ivm_match_string(ivm_span span, ivm_span* out);


/// \brief Match one line from the span
ivm_span ivm_match_line(ivm_span span, ivm_span* out);
#endif
