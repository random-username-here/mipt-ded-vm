#include "ivm/asm/symtab.h"
#include "ivm/common/parser/error.h"
#include "ivm/common/parser/span.h"
#include "ivm/common/array.h"
#include "ivm/common/macros.h"
#include <sysexits.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>

ivm_symtab ivm_symtab_new(void) {
  return ia_new_empty_array$(ivm_symtab_entry);
}

void ivm_symtab_destroy(ivm_symtab symtab) {
  
  check$(symtab, "Expected non-NULL symbol table");

  for (size_t i = 0; i < ia_length(symtab); ++i)
    if (symtab[i].destroy_param)
      symtab[i].destroy_param(symtab[i].param);

  ia_destroy_array(symtab);
}

static bool is_anonymous(const ivm_symtab_entry* entry) {
  return !entry->name.file;
}


ivm_symtab_entry* ivm_symtab_get_entry(ivm_symtab symtab, ivm_span name) {
  for (size_t i = 0; i < ia_length(symtab); ++i)
    if (!is_anonymous(&symtab[i]) && ivm_span_equals_to_span(symtab[i].name, name))
      return &symtab[i];
  return NULL;
}

ivm_symtab_entry* ivm_symtab_add_entry(ivm_symtab* symtab, ivm_symtab_entry entry) {

  if (!is_anonymous(&entry)) {
    ivm_symtab_entry* existing = ivm_symtab_get_entry(*symtab, entry.name);
    if (existing) {
      // We've got a redefenition
      char* name_string = ivm_span_get_text(entry.name);
      check$(name_string, "Failed to allocate memory to store name of the symbol");

      ivm_report(REPORT_ERROR, entry.name, "Redefenition of `%s`", name_string);
      ivm_report(REPORT_NOTE, existing->name, "Previous declaration of `%s`", name_string);

      free(name_string);
      exit(EX_DATAERR);
    }
  }

  ia_push$(symtab, entry);
  return *symtab + ia_length(*symtab) - 1;
}

// Value computation
//
// ## How does it work?
// 
// So, we have a `_get_value` function, which takes some state and an entry
// we want to compute. It, in turn, calls `compute_fn` of that entry.
// That entry can depend on values of other entries, so we provide `_get_value`
// function to it. `compute_fn` calls it when it needs symbol values.
//
// ### But what if a circular dependency happens?
//
// In that case we have such call stack:
//
// ```
// ivm_symtab_compute_values(symtab)
//  '- _get_value(<entry for foo>, ...)
//      '- <entry for foo>.compute_fn(<entry for foo>, ...)
//          |- _get_value(<entry for bar>, ...)
//          |   '- <entry for bar>.compute_fn(<entry for bar>, ...)
//          |       '- _get_value(<entry for foo>, ...) <-- Loop detected here
//          '- _get_value(<entry for baz>, ...)
// ```
//
// When reporting the error, we don't want to continue calculating values
// after circular dependency was found, so we jump over `compute_fn`-s using longjmp.
//
// ```
// ivm_symtab_compute_values(symtab) <----. longjmp
//  '- _get_value(<entry for foo>, ...) --+-<---------------.
//      '- <entry for foo>.compute_fn(<entry for foo>, ...) | longjmp
//          |- _get_value(<entry for bar>, ...) ------------+-<-----.
//          |   '- <entry for bar>.compute_fn(<entry for bar>, ...) | longjmp
//          |       '- _get_value(<entry for foo>, ...) ------------'
//          '- _get_value(<entry for baz>, ...) This is not computed
// ```
//
// ## Error reporting
//
// This thing has its custom error printing, because normal
// reporter is not meant for this.
//
// The output is like this:
//
// ```
// ╭─o .const foo @ src.s:42
// │ o .const bar @ src.s:33
// │ o .offset directive @ src.s:14
// │ o label my_label @ src.s:15
// ╰▶o .const my_const @ src.s:16     <--- state.traceback_to
//   o .const use_all @ src.s:17
// ```

typedef struct {
  jmp_buf jump_to_error_reporting;
  ivm_symtab symtab;
  bool just_found_loop;
  ivm_symtab_entry* traceback_to;
} _get_value_state;


static ivm_sym_value_t _get_value(void* _pstate, ivm_symtab_entry* entry) {

  check$(_pstate, "The state used by `get_value()` must exist");
  _get_value_state* pstate = (_get_value_state*) _pstate;

  if (entry->sym_compute_step == SYM_COMPUTE_IN_PROGRESS) {
    // We depend on entry we are computing now
    // This is a loop

    fputs("\n", stderr);
    fputs(ESC_RED ESC_BOLD "Error: " ESC_RST ESC_BOLD
          "found a circular dependency while computing values\n",
          stderr);
    fputs("\n", stderr);

    pstate->just_found_loop = true;
    pstate->traceback_to = entry;
    longjmp(pstate->jump_to_error_reporting, 0);
  }

  entry->sym_compute_step = SYM_COMPUTE_IN_PROGRESS;

  _get_value_state state = (_get_value_state) {
    .symtab = pstate->symtab,
    .just_found_loop = false,
    .traceback_to = NULL
  };

  if (!setjmp(state.jump_to_error_reporting)) {
    // We are computing the value
    check$(entry->compute_fn, "Each entry in symbol table must have a function to compute its value");
    entry->value = entry->compute_fn(entry, pstate->symtab, (void*) &state, _get_value);
  } else {
    // Loop was found, we jumped out of `compute_fn`
    check$(state.traceback_to,
           "If we've got a circular dependency, we must have an item to which to traceback");

    // Print the entry
    fputs(ESC_GRAY, stderr);
    if (state.just_found_loop)
      // First item in the output
      fputs("╭─", stderr);
    else if (state.traceback_to == entry)
      // Last item we draw line to
      fputs("╰▶", stderr);
    else if (state.traceback_to->sym_compute_step == SYM_COMPUTE_IN_PROGRESS)
      // _get_value for traceback_to had not finished, we are above it
      fputs("│ ", stderr);
    else
      fputs("  ", stderr);
    fputs("O " ESC_RST, stderr);
    
    check$(entry->print_name, "Function to print entry name must be present");
    entry->print_name(entry);

    ivm_verify_file(entry->name.file);

    fprintf(stderr,
            ESC_GRAY " @ " ESC_RST ESC_YELLOW "%s:%zu" ESC_RST "\n",
            entry->name.file->name,
            ivm_sloc_get_line(ivm_span_get_begin(entry->name)));

    entry->sym_compute_step = SYM_COMPUTE_DONE;
    pstate->traceback_to = state.traceback_to;
    longjmp(pstate->jump_to_error_reporting, 0);
  }

  // Ok, we computed the thing without failing
  entry->sym_compute_step = SYM_COMPUTE_DONE;
  return entry->value;
}


void ivm_symtab_compute_values(ivm_symtab symtab) {
  for (size_t i = 0; i < ia_length(symtab); ++i) {
    if (symtab[i].sym_compute_step == SYM_COMPUTE_DONE)
      continue;
    _get_value_state state = (_get_value_state) {
      .symtab = symtab,
      .just_found_loop = false,
      .traceback_to = NULL
    };

    if (!setjmp(state.jump_to_error_reporting)) {
      _get_value((void*) &state, &symtab[i]);
    } else {
      // A circular dependency
      fputs("\n", stderr); // Add new line after the error message
      exit(EX_DATAERR);
    }
  }
}
