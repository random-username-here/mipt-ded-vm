#include <GL/glew.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ivm/vm/crt.h"
#include "ivm/common/array.h"
#include "ivm/common/macros.h"
#include "ivm/vm/state.h"

//====[ Helper functions ]======================================================

#define INFO_LOG_BUF_SIZE 1024

/// Compile given shader
static unsigned compile_shader_or_die(GLenum type, const char* code) {

  check$(code, "Shader source to compile must be present");

  // Create new shader
  unsigned shader = glCreateShader(type);

  // Attach source to it
  int code_len = strlen(code);
  glShaderSource(shader, 1, &code, &code_len);

  // Compile
  glCompileShader(shader);

  // Check for errors
  GLint res;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
  if (res != GL_TRUE) { // There were errors
    char info[INFO_LOG_BUF_SIZE];
glGetShaderInfoLog(shader, INFO_LOG_BUF_SIZE, NULL, info);
    die$(
        "Failed to compile %s shader:\n%s",
        (type == GL_FRAGMENT_SHADER ? "fragment" : "vertex"), 
        info
    );
  }

  // Done
  return shader;
}

/// Construct program out of vertex and fragment shader
/// sources.
unsigned make_program_or_die(const char* vsh, const char* fsh) {
  
  unsigned fragment_shader = compile_shader_or_die(GL_FRAGMENT_SHADER, fsh),
           vertex_shader = compile_shader_or_die(GL_VERTEX_SHADER, vsh);

  // Assemble program
  unsigned program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  // Check for link errors
  GLint res;
  glGetProgramiv(program, GL_LINK_STATUS, &res);
  if (res != GL_TRUE) {
    char info[INFO_LOG_BUF_SIZE];
    glGetProgramInfoLog(program, INFO_LOG_BUF_SIZE, NULL, info);
    die$("Failed to link shader program together:\n%s", info);
  }

  glDeleteShader(fragment_shader);
  glDeleteShader(vertex_shader);

  // We are done
  return program;
}


//====[ Setup and callbacks ]===================================================

static void gl_debug(GLenum source, GLenum type, unsigned id, GLenum severity,
              GLsizei length, const GLchar *message, const void *vm) {

  vm_crt* crt = *((vm_crt**) vm);
  
  if (crt->vm->log_fn)
    crt->vm->log_fn(VM_LOG_INFO, "OpenGL debug message: %s", message);
}

static void gl_error(int error, const char* descr) {
  // No custom parameter alowed here, sadly
  fprintf(stdout, ESC_RED "GLFW Error: " ESC_RST "%s\n", descr);
}

static void* _render_thread(void* _crt);

vm_crt* vm_crt_new(vm_state* vm) {

  check$(vm, "You should provide a VM for which window is opened");

  // Setup the data structure

  vm_crt* crt = (vm_crt*) calloc(sizeof(vm_crt), 1);
  check$(crt, "CRT data structure must be allocated");
  crt->vm = vm;

  crt->seg_pool = ia_new_empty_array$(size_t);
  crt->segs = ia_new_empty_array$(_vm_crt_segment);
  crt->oldest_idx = -1;
  crt->newest_idx = -1;
  crt->cursor = (_vm_crt_point) { 0.5f, 0.5f };
  crt->should_exit = crt->was_closed = false;
  pthread_mutex_init(&crt->mutex, NULL);

  pthread_create(&crt->render_thread, NULL, _render_thread, crt);

  vm->crt = crt;

  return crt;
}

void vm_crt_destroy(vm_crt* crt) {
  
  check$(crt, "There should be some `vm_crt` instance to destroy");

  crt->should_exit = true;
  pthread_join(crt->render_thread, NULL);
  pthread_mutex_destroy(&crt->mutex);
}

//====[ Drawing ]===============================================================

static size_t _new_segment(vm_crt* crt) {

  if (ia_length(crt->seg_pool)) {
    // Reuse some point
    size_t item = ia_top$(crt->seg_pool);
    ia_pop$(&crt->seg_pool);
    return item;
  } else {
    // Create a new one
    _vm_crt_segment seg = { 0 };
    ia_push$(&crt->segs, seg);
    return ia_length(crt->segs) - 1;
  }
}

// Ray can scan 12000 lines per second (or 200 lines per frame at 60fps)
#define RAY_SPEED (1.0 / 12000)

static void _vm_crt_move(vm_crt* crt, float x, float y, bool stroke) {

  //printf("%smove %f %f\x1b[0m\n", (stroke ? "" : "\x1b[90m"), x, y);

  pthread_mutex_lock(&crt->mutex);

  size_t seg = _new_segment(crt);
  crt->segs[seg].start = crt->cursor;
  crt->cursor = (_vm_crt_point) { .x=x, .y = y };
  crt->segs[seg].next_idx = -1;

  double now = glfwGetTime();
  if (stroke) {

    crt->segs[seg].line_end_time = now;
    
    // Note what we cannot have line start time in `real` sense here,
    // because movement of the ray is not simulated.
    float dx = x - crt->cursor.x, dy = y - crt->cursor.y;
    
    // Approximated start time based on ray speed
    double physical_start_time = now - sqrtf(dx*dx + dy*dy) / RAY_SPEED;

    // This segment cannot begin before previous ends
    double min_start_time = crt->newest_idx != (size_t) -1
                          ? crt->segs[crt->newest_idx].line_end_time
                          : -INFINITY;

    crt->segs[seg].line_start_time = physical_start_time < min_start_time
                                   ? min_start_time
                                   : physical_start_time;

  } else {
    crt->segs[seg].line_start_time = -1;
    crt->segs[seg].line_end_time = now; // So next line won't get confused
  }

  if (crt->newest_idx != -1) {
    // Continue the linked list
    crt->segs[crt->newest_idx].next_idx = seg;
  } else {
    // There are no segments except this, so
    // this is the oldest
    crt->oldest_idx = seg;
  }

  crt->newest_idx = seg;


  pthread_mutex_unlock(&crt->mutex);
}

void vm_crt_goto(vm_crt* crt, float x, float y) {
  _vm_crt_move(crt, x, y, false);
}

void vm_crt_lineto(vm_crt* crt, float x, float y) {
  _vm_crt_move(crt, x, y, true);
}

//====[ The actuall rendering ]=================================================

#define FULL_FADE_TIME (3.0) // 1s
#define BG_COLOR 0.1, 0.1, 0.1
#define LINE_WIDTH 2.0

#define STR2(...) #__VA_ARGS__
#define STR(v) STR2(v)

const char* shader[2] = {

  // Just output the given position
  // Considering time: 1 is the current moment, 0 is the oldest rendered moment.
  // UV-s are between -1 and 1
  "#version 330 core\n"
  "layout (location = 0) in vec3 aPos;\n"
  "layout (location = 1) in float aTime;\n"
  "layout (location = 2) in vec2 aUV;\n"
  "\n"
  "out float time;\n"
  "out vec2 uv;\n"
  "\n"
  "void main() {\n"
  "  gl_Position = vec4(aPos.x * 2.0 - 1.0, -(aPos.y * 2.0 - 1.0), aPos.z, 1.0);\n"
  "  time = aTime;\n"
  "  uv = aUV;\n"
  "}\n",

  "#version 330 core\n"
  "\n"
  "out vec4 FragColor;\n"
  "uniform bool is_circle;\n"
  "\n"
  "in float time;\n"
  "in vec2 uv;\n"
  "\n"
  "const vec4 bg = vec4(" STR(BG_COLOR) ", 1.0);\n"
  "\n"
  "vec4 color(float t) {\n"
  "  const vec4 short_fade = vec4(0.7, 0.6, 0.1, 1.0);\n"
  "  float short_int = max(0, (t - 0.8) * 5);\n"
  "  const vec4 long_fade = vec4(0.8, 0.8, 0.8, 1.0);\n"
  "  float long_int = t;\n"
  "  vec4 phosphor = short_fade * short_int + long_fade * long_int;\n"
  "  return mix(bg, phosphor / 1.5, (short_int + long_int) / 2.0);\n"
  "}\n"
  "\n"
  "void main() {\n"
  "  if (!is_circle || uv.x * uv.x + uv.y * uv.y < 1.0)\n"
  "    FragColor = color(time);\n"
  "  else\n"
  "    discard;\n"
  "}\n"
};

/// \brief Draw a circle or rectangle
/// `start_time` is applied to points 0 and 1, `end_time` to points 2 and 3.
static void _draw_shape(vm_crt *crt, _vm_crt_point points[4], float start_time, float end_time, bool is_circle) {

  struct __attribute__((packed)) {
    _vm_crt_point point;
    float z;
    float time;
    _vm_crt_point uv;
  } buffer[4] = {0};

  // 0       1
  // o-------o   start
  // |\      |
  // |  \    |
  // |    \  |
  // |      \|
  // o-------o   end
  // 2       3

  for (size_t i = 0; i < 4; ++i) {
    buffer[i].point = points[i];
    buffer[i].z = 0;
  }

  buffer[0].time = buffer[1].time = start_time;
  buffer[2].time = buffer[3].time = end_time;

  buffer[0].uv = (_vm_crt_point) { -1, -1 };
  buffer[1].uv = (_vm_crt_point) {  1, -1 };
  buffer[2].uv = (_vm_crt_point) { -1,  1 };
  buffer[3].uv = (_vm_crt_point) {  1,  1 };

  glBindVertexArray(crt->vertex_array);
  glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_DYNAMIC_DRAW);

  glUseProgram(crt->shader_program);
  glUniform1i(crt->circle_uniform, is_circle ? 1 : 0);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);  
}

static void _draw_line(
      vm_crt *crt,
      _vm_crt_point begin, _vm_crt_point end, float width,
      float begin_time, float end_time
  ) {
  
  float dx = end.x - begin.x, dy = end.y - begin.y;
  float nx = -dy, ny = dx;
  float norm_len = sqrtf(nx*nx + ny*ny);
  nx /= norm_len * crt->screen_size.x; ny /= norm_len * crt->screen_size.y;
  float w2 = width / 2;

  _vm_crt_point points[4] = {
    (_vm_crt_point) { .x = begin.x + nx * w2, .y = begin.y + ny * w2 },
    (_vm_crt_point) { .x = begin.x - nx * w2, .y = begin.y - ny * w2 },
    (_vm_crt_point) { .x = end.x   + nx * w2, .y = end.y   + ny * w2 },
    (_vm_crt_point) { .x = end.x   - nx * w2, .y = end.y   - ny * w2 },
  };

  _draw_shape(crt, points, begin_time, end_time, false);
}

static void _draw_circle(
    vm_crt* crt,
    _vm_crt_point at, float r,
    float time
  ) {

  float rx = r / crt->screen_size.x;
  float ry = r / crt->screen_size.y;

  _vm_crt_point points[4] = {
    (_vm_crt_point) { .x = at.x - rx, .y = at.y - ry },
    (_vm_crt_point) { .x = at.x + rx, .y = at.y - ry },
    (_vm_crt_point) { .x = at.x - rx, .y = at.y + ry },
    (_vm_crt_point) { .x = at.x + rx, .y = at.y + ry },
  };
  _draw_shape(crt, points, time, time, true);
}

static void* _render_thread(void* _crt) {

  vm_crt* crt = (vm_crt*) _crt;

  // Do the fun stuff here
  
  //----[ Open a window ]
  
  if (!glfwInit())
    die$("Failed to init GLFW");

  glfwSetErrorCallback(gl_error);

  GLFWwindow* win = glfwCreateWindow(800, 800, "IVM", NULL, NULL);
  check$(win, "Failed to open a GLFW window");
  crt->window = win;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

  glfwMakeContextCurrent(win);

  check$(glewInit() == GLEW_OK, "GLEW should initialize");

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(&gl_debug, &crt);

  glfwShowWindow(win);

  if (crt->vm->log_fn)
    crt->vm->log_fn(VM_LOG_INFO, "CRT Window should be open now");
 
  glfwWindowHint(GLFW_SAMPLES, 4);
  glEnable(GL_MULTISAMPLE);

  //----[ Setup stuff ]

  glClearColor(BG_COLOR, 1.0);

  crt->shader_program = make_program_or_die(shader[0], shader[1]);
  crt->circle_uniform = glGetUniformLocation(crt->shader_program, "is_circle");

  glGenVertexArrays(1, &crt->vertex_array);
  glBindVertexArray(crt->vertex_array);

  glGenBuffers(1, &crt->array_buffer);
  glGenBuffers(1, &crt->elem_buffer);

  const unsigned indices[6] = {
    0, 3, 2,
    0, 1, 3
  };

  glBindBuffer(GL_ARRAY_BUFFER, crt->array_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, crt->elem_buffer);

  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_STATIC_DRAW);

  // Why it had to be void*? Why?
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*) 0);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*) (sizeof(float) * 3));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*) (sizeof(float) * 4));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  //----[ Main loop ]
  
  glfwPollEvents();

  while (!crt->should_exit && !glfwWindowShouldClose(crt->window)) {
    
    int w, h;
    glfwGetWindowSize(crt->window, &w, &h);
    glViewport(0, 0, w, h);
    crt->screen_size.x = w;
    crt->screen_size.y = h;
    double now = glfwGetTime();

    glClear(GL_COLOR_BUFFER_BIT);


    size_t seg = crt->oldest_idx;
    bool prev_stroke = false;
    float prev_end = 0;

    while (seg != -1) {
      _vm_crt_point end = crt->segs[seg].next_idx == (size_t) -1
                        ? crt->cursor
                        : crt->segs[crt->segs[seg].next_idx].start;

      float start_time = 1.0 - (now - crt->segs[seg].line_start_time) / FULL_FADE_TIME;
      float end_time = 1.0 - (now - crt->segs[seg].line_end_time) / FULL_FADE_TIME;

      if (crt->segs[seg].line_start_time > 0) {
        // If it is > 0, draw a line
        //_draw_circle(crt, crt->segs[seg].start, LINE_WIDTH/2, start_time);
        _draw_line(crt, crt->segs[seg].start, end, LINE_WIDTH, start_time, end_time);
        prev_stroke = true;
        prev_end = end_time;
      } else {
        // Else there is no line
        if (prev_stroke) { // Draw endcap on previous line
          //_draw_circle(crt, crt->segs[seg].start, LINE_WIDTH/2, prev_end);
        }
        prev_stroke = false;
      }

      seg = crt->segs[seg].next_idx;
    }
    
    //_draw_circle(crt, crt->cursor, LINE_WIDTH/2, 1.0);

    glfwSwapBuffers(crt->window);

    // Clear out old segments
    pthread_mutex_lock(&crt->mutex);
    while (crt->oldest_idx != (size_t) -1 &&
           crt->segs[crt->oldest_idx].line_end_time < now - FULL_FADE_TIME) {
      // That segment is no longer needed
      ia_push$(&crt->seg_pool, crt->oldest_idx);
      crt->oldest_idx = crt->segs[crt->oldest_idx].next_idx;
    }
    if (crt->oldest_idx == (size_t) -1)
      crt->newest_idx = -1;

    pthread_mutex_unlock(&crt->mutex);

    glfwPollEvents();
  }

  if (crt->vm->log_fn)
    crt->vm->log_fn(VM_LOG_INFO, "Rendering thread: Window was closed, performing teardown");

  //----[ Teardown ]
  
  crt->was_closed = true;

  return NULL;
}
