#pragma once

#include "base/base_core.h"

struct RGFW_window;

#define MAX_KEY_EVENTS 128
#define MAX_CHAR_INPUT_COUNT 64

struct KeyEvent {
    i32 key;
    i32 mods;
    bool pressed;
    bool repeated;
};

struct EditorInput {
    KeyEvent key_events[MAX_KEY_EVENTS];
    u32 key_event_count;
    u32 char_inputs[MAX_CHAR_INPUT_COUNT];
    u32 char_input_count;
    f32 scroll_delta;
    f64 mouse_x;
    f64 mouse_y;
    u32 window_width;
    u32 window_height;
    f32 dt_for_frame;
};

void editor_input_begin_frame(EditorInput* input);
void editor_input_snapshot_window(
    EditorInput* input,
    RGFW_window* window,
    f32 dt_for_frame
);
void editor_input_push_key_event(
    EditorInput* input,
    i32 key,
    i32 mods,
    bool pressed,
    bool repeated
);
void editor_input_push_char(EditorInput* input, u32 codepoint);
void editor_input_push_scroll(EditorInput* input, f64 yoffset);
