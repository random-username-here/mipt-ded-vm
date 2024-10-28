/// \file
/// \brief A CRT-like vector display, written using OpenGL
///

#ifndef IVM_VM_CRT
#define IVM_VM_CRT

#include "ivm/common/array.h"
#include "ivm/vm/state.h"
#include <GLFW/glfw3.h>
#include <pthread.h>

typedef struct __attribute__((packed)) {
  float x, y;
} _vm_crt_point;

typedef struct {
  // Start coordinates of the line
  // End coordinates are coordinates of next line start
  // or current cursor position if there is no next line
  _vm_crt_point start;

  // GLFW time this line was started (ray was at starting position) and ended (line wa sat ending position)
  // If this segment is not shown, times are set to -1.
  double line_start_time, line_end_time;
  // Index of the next point in the pool
  size_t next_idx;
} _vm_crt_segment;

typedef struct vm_crt {
  GLFWwindow* window;
  vm_state* vm;
  pthread_t render_thread;

  // This is a CRT, so we are drawing lines,
  // which consist of points.
  pthread_mutex_t mutex;
  ia_arr$(_vm_crt_segment) segs;
  ia_arr$(size_t) seg_pool;
  size_t oldest_idx;
  size_t newest_idx;
  _vm_crt_point cursor;

  unsigned shader_program;
  unsigned array_buffer, elem_buffer, vertex_array;
  unsigned circle_uniform;

  _vm_crt_point screen_size;

  bool should_exit;
  bool was_closed;
} vm_crt;

/// \brief Create a new CRT window
///
/// Opens the window, launches a rendering thread,
/// returns the object
///
vm_crt* vm_crt_new(vm_state* vm);

/// \brief Destroy the CRT window
///
/// This joins the rendering process and closes the window
/// (and frees the data, obviously)
///
void vm_crt_destroy(vm_crt* crt);

/// \brief Move ray to (x, y) (they are 0 to 1)
void vm_crt_goto(vm_crt* crt, float x, float y);

/// \brief Draw line from current location to (x, y)
void vm_crt_lineto(vm_crt* crt, float x, float y);

#endif

