/// \brief Error reporting
///
/// Not as fancy as my other libraries, but
/// for simple tasks it should work.
///

#ifndef IVM_ASM_PARSER_ERROR
#define IVM_ASM_PARSER_ERROR

#include "ivm/common/parser/span.h"

/// \brief Severity of the error
typedef enum {

  /// A note - usually this kind of "error"
  /// is printed after some more serious error,
  /// to make it clearer (for example, to show previous
  /// declaration)
  REPORT_NOTE,

  /// A warning - this thing does not break the program,
  /// but the programmer should look into this.
  REPORT_WARNING,

  /// The error - you know what this means
  REPORT_ERROR
} report_severity_t;


/// \brief Report some error, related to given span
/// \param severity Severity of the error
/// \param highlight Span to highlight
/// \param message Message to print on top
__attribute__((format(printf, 3, 4)))
void ivm_report(report_severity_t severity, ivm_span highlight, const char* message, ...);


/// \brief Add some details to reported error
/// \param note Those details
__attribute__((format(printf, 1, 2)))
void ivm_report_add_note(const char* note, ...);


#endif
