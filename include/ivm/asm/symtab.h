/// \brief Symbol table for string labels & constants
///
/// The actuall symbol table is just an array with linear lookup.
/// 

#ifndef IVM_ASM_SYMTAB
#define IVM_ASM_SYMTAB

#include "ivm/common/parser/span.h"
#include "ivm/common/array.h"


/// \brief Type of the values stored in the symbol table
typedef long long ivm_sym_value_t;

/// Name of the anonymous entry
///
static const ivm_span SYMTAB_ENTRY_ANONYMOUS = (ivm_span) { .file=NULL, .begin=0, .end=0 };

typedef struct ivm_symtab_entry ivm_symtab_entry;

typedef ia_arr$(ivm_symtab_entry) ivm_symtab;

/// \brief Entry in the symbol table
///
/// Each value has a name (span points to .const name
/// or to label name) and computed value.
typedef struct ivm_symtab_entry {

  /// Name of the entry
  ivm_span name;

  /// Its value. It is known only after the constant computation step
  ivm_sym_value_t value;

  // Things for compute step

  /// Function to compute value of the constant
  ivm_sym_value_t (*compute_fn)(
      struct ivm_symtab_entry* entry,
      ivm_symtab symtab,
      void* state,
      ivm_sym_value_t (*get_value)(void* state, ivm_symtab_entry* entry)
  );

  /// Print name of this entry (like `.offset directive` or `.const foobar`)
  /// Used when printing about variable loops
  void (*print_name)(struct ivm_symtab_entry* entry);

  /// Destroy the param
  /// This function can be `NULL`, if there is nothing to free.
  void (*destroy_param)(void* param);

  /// Parameter for the compute function
  /// For example, for .const this will be the expression
  void* param;

  /// State of this 
  enum {
    SYM_COMPUTE_NOT_DONE,
    SYM_COMPUTE_IN_PROGRESS,
    SYM_COMPUTE_DONE
  } sym_compute_step;

} ivm_symtab_entry;



/// \brief Create a new symbol table
ivm_symtab ivm_symtab_new(void);


/// \brief Destroy the symbol table no longer needed
void ivm_symtab_destroy(ivm_symtab symtab);


/// \brief Add new entry to the symbol table
/// \returns Pointer to the added entry
ivm_symtab_entry* ivm_symtab_add_entry(ivm_symtab* symtab, ivm_symtab_entry entry);


/// \brief Find entry in the symbol table
/// \param symtab The symbol table
/// \param name Name of the variable to search for
/// \returns Found entry or `NULL` if it was not found
ivm_symtab_entry* ivm_symtab_get_entry(ivm_symtab symtab, ivm_span name);


/// \brief Compute values of the symbol table entries
/// This function may exit the program in case some constants form infinite loop
void ivm_symtab_compute_values(ivm_symtab symtab);

#endif
