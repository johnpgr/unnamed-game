#include <string.h>

#include "draw/draw_core.h"

PushCmdBuffer create_push_cmd_buffer(Arena* arena, u32 capacity) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(capacity > 0, "Push command buffer capacity must be non-zero!");

    PushCmdBuffer result = {};
    result.base = (u8*)push_size(arena, capacity);
    result.capacity = capacity;
    return result;
}

void push_cmd_buffer_reset(PushCmdBuffer* buffer) {
    ASSERT(buffer != nullptr, "Push command buffer must not be null!");

    buffer->used = 0;
    buffer->cmd_count = 0;
}

internal PushCmd* push_cmd(
    PushCmdBuffer* buffer,
    CmdType type,
    u32 size,
    u32 alignment = 8
) {
    ASSERT(buffer != nullptr, "Push command buffer must not be null!");
    ASSERT(size >= sizeof(PushCmd), "Push command size must include header!");
    ASSERT(
        is_pow2(alignment),
        "Push command alignment must be a power of two!"
    );

    u32 aligned_used = (buffer->used + alignment - 1) & ~(alignment - 1);
    u32 required = aligned_used + size;
    ASSERT(required <= buffer->capacity, "Push command buffer overflow!");

    PushCmd* result = (PushCmd*)(buffer->base + aligned_used);
    memset(result, 0, size);
    result->type = type;
    result->size = size;

    buffer->used = required;
    ++buffer->cmd_count;
    return result;
}

void push_clear(PushCmdBuffer* buffer, vec4 color) {
    CmdClear* cmd =
        (CmdClear*)push_cmd(buffer, CmdType_Clear, sizeof(CmdClear));
    cmd->color = color;
}

void push_rect(
    PushCmdBuffer* buffer,
    vec2 center,
    vec2 size,
    vec4 color
) {
    CmdRect* cmd = (CmdRect*)push_cmd(buffer, CmdType_Rect, sizeof(CmdRect));
    cmd->center = center;
    cmd->size = size;
    cmd->color = color;
}
