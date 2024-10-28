/// \file
/// \brief Dynamic array library, revision 2
///
/// It works somewhat like this:
///
/// ```
///                                                      ┌─ zero byte for string
///                                                      │  functions to work
///                                                      ▼
/// -----------------------------------------------------------
///   | len | availiable | element 1 | element 2 | ... | \0 |
/// -----------------------------------------------------------
///   ▲                        ▲ 
///   │                        └─ pointer given to you points here
///   │
///   └─ beginning of allocated memory
/// ```
///
/// *(I just love making those diagrams)*
///
/// Contrary to previous version, this uses way less macros
/// and more separate functions, reducing amount of possible macro errors.
///

#ifndef ISTD_ARR
#define ISTD_ARR

#include <stddef.h>
#include <stdalign.h>
#include "ivm/common/macros.h"

//------ Getter functions ----------------------------------------------------//


/// \brief Get length of given dynamic array
///
/// If given `arr` is `NULL`, then this function will return `0`.
/// This is well-defined behaviour.
///
size_t ia_length(const void* arr);


/// \brief Number of items which can be fitted into given array without reallocation
///
/// This value will be allways >= `ia_length()`.
///
/// Again, this function will accept `arr = NULL` and return `0`.
///
size_t ia_avail(const void* arr);


//------ Array creation/destruction ------------------------------------------//

/// \brief A wrapper to use when typing arrays.
///
/// So instead `int*` you would use `ia_arr(int)`, which compiles to the
/// same thing for now (I would love to change that, to prevent running
/// `ia_**` functions on unknown pointers, but I see no way to achive that).
///
#define ia_arr$(type) type*

/// \brief Allocate an array
///
/// \note Use `ia_new_empty_array()`/`ia_new_array_of()`/
///       `ia_new_array_for()` instead when possible.
///
/// This function allocates an array with space for `prealloc` items,
/// initializes first `len` items with zeroes. Items are `item_size` bytes
/// each.
///
/// Just like `calloc()` this thing can fail when given amount of memory
/// cannot be given to you. In that case, it will panic.
///
/// If you set `len` > `prealloc`, then internally `prealloc` is set to `len`,
/// so you may call `ia_alloc_array(0, 100, sizeof(int))` and get an array
/// of 100 ints.
///
/// \param prealloc  Number of items for which space must be reserved
/// \param len       Number of items which are zero-initialized.
///                  Returned array has that number of items.
/// \param item_size Size of each item in array, in bytes
///
void* ia_alloc_array(size_t prealloc, size_t len, size_t item_size);


/// \brief Allocate empty array for elements of given `type`.
///
/// May return `NULL` if your system has totally ran out of memory.
///
#define ia_new_empty_array$(type) \
  ((type*) ia_alloc_array(0, 0, sizeof(type)))


/// \brief Allocate array with `amount` of zero-initialized items of given `type`
///
/// Again, if you do not have enough memory this will return `NULL`.
///
#define ia_new_array_of$(amount, type) \
  ((type*) ia_alloc_array(0, (amount), sizeof(type)))


/// \brief Allocate array with preallocated space for `amount` of items of given `type`.
///
/// May return `NULL` if that array cannot be allocated.
///
#define ia_new_array_for$(amount, type) \
  ((type*) ia_alloc_array((amount), 0, sizeof(type)))


/// \brief Free memory of given array.
///
/// Will happily accept `NULL` and do nothing, like `free()`
///
void ia_destroy_array(void* array);


//------ Common operations ---------------------------------------------------//


/// \internal
/// Internals of `ia_push()` macro.
void _ia_generic_push(void** array, const void* item, size_t item_size);


/// \brief Push element into given array
///
/// `sizeof(array item)` bytes of `item` will be `memcpy`-ed into that array.
/// To do that this macro creates a scoped variable `_ia_referencable_item`, to
/// which item is assigned. I 
///
/// If item is not of array type (pushing `long long` into `int` array), then
/// it will be explicitly cast to `int`.
/// 
/// If `*array` is `NULL`, this will create a new array and assign it to `*array`.
///
/// \param array Pointer to array to push item into (`&arr`, not just `arr`).
/// \param  item Item to push into array. 
///
#define ia_push$(array, item) do {\
    typeof(**array) _ia_referencable_item = (item);\
    _ia_generic_push((void**) (array), &_ia_referencable_item, sizeof(_ia_referencable_item));\
  } while(0)


void _ia_generic_pop(void** array, size_t item_size);


/// \brief Pop last item from given array.
#define ia_pop$(array) _ia_generic_pop((void**) (array), sizeof(typeof(**array)))

#define _ia_top$(array, temp_name) ({\
      typeof(array) temp_name = (array);\
      check$(ia_length(temp_name), "Array must have an item to get");\
      temp_name[ia_length(temp_name)-1];\
    })

/// \brief Return top item of given array
#define ia_top$(array) _ia_top$(array, concat$(_ia_top_temp__, __COUNTER__))


/// \brief Remove all elements from the array
void _ia_clear(void** array);

#define ia_clear$(array) _ia_clear((void**) (array))

#endif
