/// \brief A little calculator used for computing constants and such
///

#ifndef IVM_ASM_CALC
#define IVM_ASM_CALC

#include "ivm/asm/symtab.h"

/// \brief Add an entry to symbol table with this expression
///
/// Considering error reports: they will be shown like this:
///
/// ```
/// Error: operator `<->` does not exist
///    .--[ foo.s ]
///  1 | .const FOO 1 <-> 2
///    |              ^^^
/// 
/// Note: this happened during computation of `.const FOO`
///    .--[ foo.s ]
///  1 | .const FOO 1 <-> 2
///    |        ^^^
/// ```
///
/// Here you can see where `name` amd `ref` are used.
///
/// Also note what you cannot glue unary and binary operators together.
/// This will not work: `3+-2`, because `+-` will be interpreted as one operator.
/// If you need this to work, add a space between those two operators, 
/// like this: `3 + -2`.
///
/// \warning This returns not fully intialized entry!
///
/// \param ref Span, which will be shown in the error report if something goes wrong.
///            If this thing is not anonymous, it is printed when reporting circular dependencies.
///            (for example: `.const declaration foobar`, where ref is `foobar`)
/// \param descr Name to show in error report when something goes wrong
/// \param expr Expression to compute
/// \param symbol_table Table from which to resolve symbol names
/// \param is_anonymous If true, result will be added into symbol table as anonymous expression.
/// \returns Entry in the symbol table, corresponding to that expression
ivm_symtab_entry* ivm_add_computed_entry(ivm_span ref, const char* descr,
                                         ivm_span expr, ivm_symtab* symbol_table,
                                         bool is_anonymous);

/// \brief Compute the expression, using already computed symbol table
/// Params are the same
ivm_sym_value_t ivm_compute_expression(ivm_span ref, const char* descr,
                                       ivm_span expr, ivm_symtab* symbol_table);
#endif
