#include "ivm/common/parser/error.h"
#include "ivm/common/parser/span.h"
#include "ivm/common/macros.h"
#include <stdio.h>
#include <stdarg.h>

static const char* report_colors[] = {
  [REPORT_NOTE] = ESC_GREEN,
  [REPORT_WARNING] = ESC_YELLOW,
  [REPORT_ERROR] = ESC_RED
};

static const char* report_names[] = {
  [REPORT_NOTE] = "Note",
  [REPORT_WARNING] = "Warning",
  [REPORT_ERROR] = "Error"
};

__attribute__((format(printf, 3, 4)))
void ivm_report(report_severity_t severity, ivm_span highlight, const char* message, ...) {

  check$(message, "Message for error report must");
  ivm_verify_file(highlight.file);

  const char* color = report_colors[severity];

  fprintf(stderr, "\n");
  fprintf(stderr, ESC_BOLD "%s%s: " ESC_RST ESC_BOLD, color, report_names[severity]);

  va_list args;
  va_start(args, message);
  vfprintf(stderr, message, args);
  va_end(args);

  fprintf(stderr, ESC_RST "\n");
  fprintf(stderr, "\n");
  fprintf(stderr,
          ESC_GRAY "    ╭──[ " ESC_RST ESC_UNDERLINE "%s" ESC_RST ESC_GRAY " ]\n" ESC_RST,
          highlight.file->name);

  ivm_sloc end = ivm_span_get_end(highlight);
  end.offset--;

  size_t start_line = ivm_sloc_get_line(ivm_span_get_begin(highlight)),
         start_col  = ivm_sloc_get_column(ivm_span_get_begin(highlight)),
         end_line   = ivm_sloc_get_line(end),
         end_col    = ivm_sloc_get_column(end);

  if (highlight.begin == highlight.end) {
    // We draw an arrow
    fprintf(stderr, ESC_GRAY "%3zu │ " ESC_RST, start_line+1);

    const char* line = highlight.file->contents + highlight.file->line_start_offsets[start_line];
    for (size_t i = 0; line[i] != '\n' && line[i] != '\0'; ++i)
      fputc(line[i], stderr);
    fputc('\n', stderr);

    fprintf(stderr, ESC_GRAY "    │ " ESC_RST);

    for (size_t i = 0; i < start_col; ++i)
      fputc(' ', stderr);

    fprintf(stderr, "%s↑" ESC_RST "\n", color);

  } else if (start_line == end_line) {
    fprintf(stderr, ESC_GRAY "%3zu │ " ESC_RST, start_line+1);

    const char* line = highlight.file->contents + highlight.file->line_start_offsets[start_line];
    for (size_t i = 0; i < start_col; ++i)
      fputc(line[i], stderr);
    fprintf(stderr, "%s", color);
    for (size_t i = start_col; i <= end_col; ++i)
      fputc(line[i], stderr);
    fprintf(stderr, ESC_RST);
    for (size_t i = end_col+1; line[i] != '\n' && line[i] != '\0'; ++i)
      fputc(line[i], stderr);
    fprintf(stderr, "\n");

    fprintf(stderr, ESC_GRAY "    │ " ESC_RST);

    for (size_t i = 0; i < start_col; ++i)
      fputc(' ', stderr);
    fprintf(stderr, "%s", color);
    for (size_t i = start_col; i <= end_col; ++i)
      fputs("^", stderr);
    fprintf(stderr, ESC_RST "\n");
  } else {
    // Multiline span
    // Here we go

    for (size_t ln = start_line; ln <= end_line; ++ln) {
      const char* line = highlight.file->contents + highlight.file->line_start_offsets[ln];

      if (ln == start_line || ln == end_line) {

        bool is_begin = ln == start_line;
        const char* begin_angle = is_begin ? "╭" : "╰";
        size_t column = is_begin ? start_col : end_col;

        fprintf(stderr, ESC_GRAY "%3zu │ %s%s " ESC_RST, ln+1, is_begin ? "" : color, is_begin ? " " : "│");
        if (!is_begin) fputs(color, stderr);
        for (size_t i = 0; line[i] && i < column; ++i)
          fputc(line[i], stderr);

        fputs(is_begin ? color : ESC_RST, stderr);
        for (size_t i = column; line[i] != '\n' && line[i] != '\0'; ++i)
          fputc(line[i], stderr);
        fprintf(stderr, "\n");

        fprintf(stderr, ESC_RST ESC_GRAY "    │ %s%s─", color, begin_angle);
        for (size_t i = 0; i < column; ++i)
          fputs("─", stderr);
        fputs("^", stderr);
        fprintf(stderr, ESC_RST "\n");
      } else {
        fprintf(stderr, ESC_GRAY "%3zu │ %s│ ", ln+1, color);
        for (size_t i = 0; line[i] != '\n' && line[i] != '\0'; ++i)
          fputc(line[i], stderr);
        fprintf(stderr, ESC_RST "\n");
      }
    }
  }
  fprintf(stderr, ESC_GRAY "    │\n" ESC_RST);
  fprintf(stderr, "\n");
}

#define ESC_MOVE_UP_ONE_LINE "\x1b[F"

__attribute__((format(printf, 1, 2)))
void ivm_report_add_note(const char* note, ...) {
  fputs(ESC_MOVE_UP_ONE_LINE ESC_GRAY "    = " ESC_RST ESC_BOLD "note: " ESC_RST, stderr);
  va_list args;
  va_start(args, note);
  vfprintf(stderr, note, args);
  va_end(args);
  fputs("\n\n", stderr);
}



