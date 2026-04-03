#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#include "ivm/vm/crt.h"
#include "ivm/common/array.h"
#include "ivm/common/macros.h"
#include "ivm/vm/mainloop.h"
#include "ivm/vm/state.h"

#define TIME_BETWEEN_TICKS (1.0 / 30)
#define FULL_FADE_TIME 0.2 
#define BG_COLOR 0.1, 0.1, 0.1
#define LINE_WIDTH 2.0

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

  size_t seg = _new_segment(crt);
  crt->segs[seg].start = crt->cursor;
  crt->cursor = (_vm_crt_point) { .x=x, .y = y };
  crt->segs[seg].next_idx = -1;

  double now = vm_time_ns() / 1.0e9;
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
}

void vm_crt_goto(vm_crt* crt, float x, float y) {
  _vm_crt_move(crt, x, y, false);
}

void vm_crt_lineto(vm_crt* crt, float x, float y) {
  _vm_crt_move(crt, x, y, true);
}

//====[ The actuall rendering ]=================================================

#define STR2(...) #__VA_ARGS__
#define STR(v) STR2(v)

typedef struct {
  uint16_t scancode;
  uint8_t action, mods;
} keyboard_intr_data;

static void _setup_key_interrupt(vm_state* vm, void* _data) {
  keyboard_intr_data* data = (keyboard_intr_data*) _data;
  vm->kb_scancode = data->scancode;
  vm->kb_mods = data->mods;
  vm->kb_action = data->action;
  free(data);
}

static int l_collect_mods(void) {
    int mods = 0;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        mods |= 1;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        mods |= 2;
    // TODO: other modifiers
    return mods;
}

static void l_trigger_key_intr(vm_crt *crt, int key, int action, int mods) {
    keyboard_intr_data* data = (keyboard_intr_data*) calloc(1, sizeof(keyboard_intr_data));

    data->scancode = (uint16_t) key;
    data->action = (uint8_t) action;
    data->mods = (uint8_t) mods;

    vm_state_trigger_interrupt(crt->vm, (vm_interrupt) {.data = data, .type=VM_INTR_KEY, .setup_state=_setup_key_interrupt });
}

const char *fs = 
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "const float renderWidth = 800;\n"
    "const float renderHeight = 800;\n"
    "float weight[3] = float[](0.25, 0.5, 0.25);\n"
    "void main()\n"
    "{\n"
    "   vec3 texelColor = vec3(0.0);\n"
    "   for (int i = -1; i <= 1; i++) {\n"
    "       texelColor += texture(texture0, fragTexCoord + vec2(float(i)/renderWidth, 0.0)).rgb*weight[i + 1];\n"
    "   }\n"
    "   finalColor = vec4(texelColor, 1.0);\n"
    "}\n";

// due to raylib having global state, who cares
static RenderTexture rt;
static Shader postproc;

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
    crt->prev_update_time = 0;
    vm->crt = crt;
    crt->keys = ia_new_empty_array$(int);

    // Open window

    InitWindow(800, 800, "IVM");
    rt = LoadRenderTexture(800, 800);
    postproc = LoadShaderFromMemory(NULL, fs);
    return crt;
}

void vm_crt_loop(vm_crt *crt) {
    //----[ Main loop ]

    if (!crt || crt->was_closed)
        return;

    double now = vm_time_ns() / 1.0e9;
    if (crt->prev_update_time + TIME_BETWEEN_TICKS > now)
        return;
    crt->prev_update_time = now;

    PollInputEvents();

    if(crt->should_exit || WindowShouldClose()) {
        if (crt->vm->log_fn)
            crt->vm->log_fn(VM_LOG_INFO, "Rendering thread: Window was closed, performing teardown");
        crt->was_closed = true;
        crt->vm->should_die = true;
        CloseWindow();
        return;
    }

    int mods = l_collect_mods();

    int key;
    while ((key = GetKeyPressed())) {
        l_trigger_key_intr(crt, key, 1, mods);
        ia_push$(&crt->keys, key);
    }
    
    size_t kpos = 0;
    for (size_t i = 0; i < ia_length(crt->keys); ++i) {
        if (!IsKeyDown(crt->keys[i])) {
            l_trigger_key_intr(crt, crt->keys[i], 0, mods);
        } else {
            crt->keys[kpos++] = crt->keys[i];
        }
    }
    ia_resize$(&crt->keys, kpos);

    BeginTextureMode(rt);

    size_t seg = crt->oldest_idx;

    int w = GetScreenWidth(), h = GetScreenHeight();
    while (seg != (size_t) -1) {
        _vm_crt_point end = crt->segs[seg].next_idx == (size_t) -1
            ? crt->cursor
            : crt->segs[crt->segs[seg].next_idx].start;
        _vm_crt_point start = crt->segs[seg].start;

        float start_time = 1.0 - (now - crt->segs[seg].line_start_time) / FULL_FADE_TIME;
        float end_time = 1.0 - (now - crt->segs[seg].line_end_time) / FULL_FADE_TIME;

        if (crt->segs[seg].line_start_time > 0) {
            // If it is > 0, draw a line
            DrawLine(start.x * w, start.y * h, end.x * w, end.y * h, ColorLerp(BLACK, GetColor(0xe38e4dff), end_time));
        }
        seg = crt->segs[seg].next_idx;
    }

    EndTextureMode();

    BeginDrawing();
    ClearBackground(GetColor(0x333333ff));
    BeginShaderMode(postproc);
    DrawTextureRec(rt.texture, (Rectangle) { 0, 0, rt.texture.width, -rt.texture.height }, (Vector2) {0, 0}, WHITE);
    EndShaderMode();
    EndDrawing();


    // Clear out old segments
    while (crt->oldest_idx != (size_t) -1 &&
            crt->segs[crt->oldest_idx].line_end_time < now - FULL_FADE_TIME) {
        // That segment is no longer needed
        ia_push$(&crt->seg_pool, crt->oldest_idx);
        crt->oldest_idx = crt->segs[crt->oldest_idx].next_idx;
    }
    if (crt->oldest_idx == (size_t) -1)
        crt->newest_idx = -1;
}

void vm_crt_destroy(vm_crt* crt) {
    if (!crt) return;  
    crt->should_exit = true;
    vm_crt_loop(crt);
}

