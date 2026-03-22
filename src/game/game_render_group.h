#pragma once

#include "base/core.h"
#include "base/lane.h"
#include "game/game_math.h"

#define PUSH_BUFFER_SIZE_PER_LANE (4 * MB)

enum RenderEntryType : u32 {
    render_entry_type_clear,
    render_entry_type_rect,
};

struct RenderEntryHeader {
    RenderEntryType type;
    u32 size;
    f32 sort_key;
};

struct RenderEntryClear {
    RenderEntryHeader header;
    vec4 color;
};

struct RenderEntryRect {
    RenderEntryHeader header;
    vec2 p;
    f32 width;
    f32 height;
    vec4 color;
};

struct LanePushBuffer {
    u8 *base;
    u32 used;
    u32 capacity;
    u32 entry_count;
};

struct RenderCommands {
    LanePushBuffer lane_buffers[MAX_LANES];
    u32 active_lane_count;
    u32 screen_width;
    u32 screen_height;
};

#define push_render_entry(commands, type_enum, Type)                           \
    (Type *)push_render_entry_((commands), sizeof(Type), (type_enum))

internal void *push_render_entry_(
    RenderCommands *commands,
    u32 size,
    RenderEntryType type
) {
    assert(commands != nullptr, "Render commands must not be null!");
    u32 idx = lane_idx();
    assert(idx < commands->active_lane_count, "Lane index out of range!");

    LanePushBuffer *buffer = commands->lane_buffers + idx;
    assert(buffer->base != nullptr, "Lane push buffer must not be null!");
    assert(buffer->used + size <= buffer->capacity, "Push buffer overflow!");

    RenderEntryHeader *header =
        (RenderEntryHeader *)(buffer->base + buffer->used);
    header->type = type;
    header->size = size;
    header->sort_key = 0.0f;

    buffer->used += size;
    ++buffer->entry_count;
    return header;
}

internal void push_clear(RenderCommands *commands, vec4 color) {
    RenderEntryClear *entry =
        push_render_entry(commands, render_entry_type_clear, RenderEntryClear);
    entry->color = color;
}

internal void push_rect(
    RenderCommands *commands,
    vec2 p,
    f32 width,
    f32 height,
    vec4 color,
    f32 sort_key
) {
    RenderEntryRect *entry =
        push_render_entry(commands, render_entry_type_rect, RenderEntryRect);
    entry->p = p;
    entry->width = width;
    entry->height = height;
    entry->color = color;
    entry->header.sort_key = sort_key;
}

internal void reset_render_commands(RenderCommands *commands) {
    assert(commands != nullptr, "Render commands must not be null!");

    for(u32 lane_index = 0; lane_index < commands->active_lane_count;
        ++lane_index) {
        LanePushBuffer *buffer = commands->lane_buffers + lane_index;
        buffer->used = 0;
        buffer->entry_count = 0;
    }
}
