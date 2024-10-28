#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ivm/common/array.h"
#include "ivm/common/macros.h"

/// The actual array object
/// When using all methods of this library, you get/pass pointer to the `data`.
typedef struct {

  /// Number of items currently in the array
  size_t length;

  /// Number of items which can be fitted into this array without reallocation.
  size_t availiable;

  /// Array with element data
  alignas(alignof(max_align_t)) char data[];
} _ia_actual_array_t;


/// Get array struct of that pointer
_ia_actual_array_t* actual_array(const void* arr) {
  assert(arr);
  return (_ia_actual_array_t*) (((char*) arr) - offsetof(_ia_actual_array_t, data));
}

char* zero_byte(const void* array, size_t item_size) {
  _ia_actual_array_t* actual = actual_array(array);
  return actual->data + actual->length * item_size;
}

/// Get length of array with elements starting at that pointer
size_t ia_length(const void* arr) {
  if (!arr) return 0;
  return actual_array(arr)->length;  
}


/// Get number of avail. space for items in that array.
size_t ia_avail(const void *arr) {
  if (!arr) return 0;
  return actual_array(arr)->availiable;
}


/// Allocate array with LEN elems and space for PREALLOC.
void* ia_alloc_array(size_t prealloc, size_t len, size_t item_size) {

  assert(item_size);

  if (prealloc < len)
    prealloc = len;

  _ia_actual_array_t* arr = 
    (_ia_actual_array_t*) calloc(
        1, sizeof(_ia_actual_array_t) + item_size * prealloc + 1);
  if (!arr) // ENOMEM is set by calloc
    die$(
        "Failed to allocate space for %zu items of size %zu bytes",
        prealloc, item_size
      );

  arr->length = len;
  arr->availiable = prealloc;
  
  return arr->data;
}


/// Free that array
void ia_destroy_array(void *array) {

  if (!array) // Not freeing null.
    return;

  free(actual_array(array));
}


/// Make sure array has space for `avail` items.
static void array_must_have_space(void** array, size_t avail, size_t item_size) {

  assert(array);

  _ia_actual_array_t* arr = array ? actual_array(*array) : NULL;

  while (ia_avail(arr->data) < avail) {
    size_t nw = ia_avail(arr->data) * 3 / 2 + 1;
    // NOTE: this can result if memory leak if realloc returns NULL
    //       (but we panic in that case, so it does not matter)
    arr = (_ia_actual_array_t*) realloc(
        arr,
        nw * item_size + 1 + sizeof(_ia_actual_array_t)
    );
    arr->availiable = nw;
    if (!arr)
      die$(
          "Failed to grow array with %zu-byte items to be "
          "able to fit %zu of them",
          item_size, arr->availiable
      );
  }

  *array = arr->data;
}

void _ia_generic_push(void** array, const void* item, size_t item_size) {

  assert(array);
  assert(item);

  array_must_have_space(array, ia_length(*array)+1, item_size);
  _ia_actual_array_t* arr = actual_array(*array);
  memcpy(arr->data + item_size * arr->length, item, item_size);
  arr->length++;
  *zero_byte(*array, item_size) = '\0';
}

void _ia_generic_pop(void **array, size_t item_size) {

  assert(array);

  if (ia_length(*array) == 0)
    die$("Cannot pop() from empty array");

  _ia_actual_array_t* arr = actual_array(*array);
  arr->length--;
  *zero_byte(*array, item_size) = '\0';
}

void _ia_clear(void** array) {

  assert(array);
  _ia_actual_array_t* arr = actual_array(*array);
  arr->length = 0;

}
