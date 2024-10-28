#include "ivm/common/parser/match.h"
#include "ivm/common/parser/error.h"
#include "ivm/common/parser/span.h"
#include "ivm/common/parser/span.h"
#include "ivm/common/macros.h"
#include <ctype.h>
#include <regex.h>
#include <stdlib.h>
#include <sysexits.h>

/// Return an empty span at the end of given one
static ivm_span end_span(ivm_span span) {
  return (ivm_span) {
    .file = span.file,
    .begin = span.end,
    .end = span.end
  };
}

static ivm_span start_span(ivm_span span) {
  return (ivm_span) {
    .file = span.file,
    .begin = span.begin,
    .end = span.begin
  };
}

ivm_span ivm_trim_ws(ivm_span span) {

  for (size_t i = 0; i < ivm_span_get_length(span); ++i)
    if (!isspace(ivm_span_get_char(span, i)))
      return ivm_span_slice(span, i, ivm_span_slice_to_end);

  // Everything was spaces, return empty span at the end
  return end_span(span);
}


ivm_span ivm_match_comment(ivm_span span, ivm_span* out) {
  
  // Comments are in the form of `// something \n`
  // (no multiline comments for now)  

  // Nothing to match
  if (ivm_span_get_length(span) < 2)
    goto no_comment;

  // No comment here
  if (ivm_span_get_char(span, 0) != '/' || ivm_span_get_char(span, 1) != '/')
    goto no_comment;

  for (size_t i = 2; i < ivm_span_get_length(span); ++i)
    if (ivm_span_get_char(span, i) == '\n') {
      // We found the end of the comment
      if (out)
        *out = ivm_span_slice(span, 1, i); // remove `;`
      return ivm_span_slice(span, i, ivm_span_slice_to_end);
    }

  // No \n occured, we got to the end
  // But the are in comment!

  if (out)
    *out = ivm_span_slice(span, 1, ivm_span_slice_to_end);
  return end_span(span); 

 no_comment:
  if (out) {
    out->file = span.file;
    out->begin = out->end = span.end;
  }
  return span;
}


ivm_span ivm_trim_ws_and_comments(ivm_span span) {

  span = ivm_trim_ws(span);
  while (!ivm_span_is_empty(span) && ivm_span_get_char(span, 0) == ';')
    span = ivm_trim_ws(ivm_match_comment(span, NULL));
  
  return span;
}


// The fun part
ivm_span ivm_match_regex(ivm_span span, const regex_t* regex, ivm_span* out) {


  check$(regex, "Regex must be provided to match by it");
  ivm_verify_file(span.file);

  if (ivm_span_is_empty(span)) {
    *out = span;
    return span;
  }

  check$(span.begin < span.file->size, "Given span must fit into the file");
  check$(span.end <= span.file->size, "Given span must fit into the file");

  regmatch_t full_match = (regmatch_t) {
    .rm_so = 0,
    .rm_eo = ivm_span_get_length(span)
  };

  // This uses BSD extension, but it is required here
  int res = regexec(regex, span.file->contents + span.begin, 1, &full_match, REG_STARTEND);
  // HACK: Not the best thing, but this code is used for assembler only.
  check$(res != REG_ESPACE, "Regex matcher ran out of memory");

  if (res == REG_NOMATCH) {
    if (out)
      *out = start_span(span);
    return span;
  }

  if (out)
    *out = ivm_span_slice(span, full_match.rm_so, full_match.rm_eo);
  return ivm_span_slice(span, full_match.rm_eo, ivm_span_slice_to_end);
}


static regex_t identifier_regex;
static regex_t number_regex;
static regex_t not_nl;

void ivm_compile_regex_or_die(regex_t* out, const char* pattern, int flags) {
  int err = regcomp(out, pattern, flags);

  if (err) {
    size_t errbuf_size = regerror(err, out, NULL, 0);
    char* errbuf = (char*) calloc(errbuf_size, 1);
    check$(errbuf, "Failed to allocate buffer to put regex compilation failure message");
    regerror(err, out, errbuf, errbuf_size);
    die$("Failed to compile regex `%s`: %s", pattern, errbuf);
  }
}

__attribute__((constructor))
static void _compile_regexes(void) {

  // Just a normal C identifier
  ivm_compile_regex_or_die(&identifier_regex,
                           "^[A-Za-z_][A-Za-z0-9_]*",
                           REG_EXTENDED);

  // Number regex, allowing prefixes
  // Note what 0x and such must be before matching of regular numbers,
  // or this will match 0 in 0xff.
  ivm_compile_regex_or_die(&number_regex,
                           "^(0x[0-9a-fA-F]+)|(0o[0-7]+)|(0b[01]+)|([0-9]+(\\.[0-9]+)?)",
                           REG_EXTENDED);

  ivm_compile_regex_or_die(&not_nl, "^[^\n]*", REG_EXTENDED);
}

ivm_span ivm_match_identifier(ivm_span span, ivm_span* out) {
  return ivm_match_regex(span, &identifier_regex, out);
}

ivm_span ivm_match_number(ivm_span span, ivm_span* out) {
  return ivm_match_regex(span, &number_regex, out);
}

ivm_span ivm_match_string(ivm_span span, ivm_span* out) {
  check$(out, "Place to output matched string must be present");

  if (ivm_span_is_empty(span)) {
    *out = start_span(span);
    return span;
  }
  char quote = ivm_span_get_char(span, 0);
  if (quote != '"' || quote != '\'') {
    *out = start_span(span);
  }

  bool esc = false;
  size_t end = 1;
  while (esc || ivm_span_get_char(span, end) != quote) {
    ++end;
    if (end == ivm_span_get_length(span)) {
      ivm_report(REPORT_ERROR, span, "Missing closing `%c` for the string", quote);
      exit(EX_DATAERR);
    }
  }
  ++end; // The quote

  *out = ivm_span_slice(span, 0, end);
  return ivm_span_slice(span, end, ivm_span_slice_to_end);
}

ivm_span ivm_match_line(ivm_span span, ivm_span* out) {
  span = ivm_match_regex(span, &not_nl, out);

  // Remove the \n
  if (!ivm_span_is_empty(span))
    span = ivm_span_slice(span, 1, ivm_span_slice_to_end);

  return span;
}
