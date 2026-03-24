#pragma once

#include "base/base_arena.h"

enum CmdType : u32 {
    CmdType_None,
    CmdType_Clear,
    CmdType_Rect,
};

struct PushCmd {
    CmdType type;
    u32 size;
};

struct CmdClear {
    PushCmd header;
    vec4 color;
};

struct CmdRect {
    PushCmd header;
    vec2 center;
    vec2 size;
    vec4 color;
};

struct PushCmdBuffer {
    u8* base;
    u32 capacity;
    u32 used;
    u32 cmd_count;
};

PushCmdBuffer create_push_cmd_buffer(Arena* arena, u32 capacity);
void push_cmd_buffer_reset(PushCmdBuffer* buffer);
void push_clear(PushCmdBuffer* buffer, vec4 color);
void push_rect(
    PushCmdBuffer* buffer,
    vec2 center,
    vec2 size,
    vec4 color
);
