#pragma once

#include "input.h"
#include "push_cmds.cpp"
#include "text_buffer.h"

struct EditorState {
    Arena* permanent_arena;
    Arena* transient_arena;
    TextDocument* document;
    i32 cursor_column;
    i32 desired_column;
    i32 cursor_row;
    f32 blink_timer;
    bool dirty;
};

internal void init_editor_state(
    EditorState* state,
    Arena* permanent_arena,
    Arena* transient_arena
) {
    assert(state != nullptr, "Editor state must not be null!");
    assert(permanent_arena != nullptr, "Permanent arena must not be null!");
    assert(transient_arena != nullptr, "Transient arena must not be null!");

    *state = {};
    state->permanent_arena = permanent_arena;
    state->transient_arena = transient_arena;
    state->document = text_document_create(permanent_arena, {});
    state->dirty = true;
}

// Convert cursor (row, col) to a logical byte offset in the document.
internal u64 cursor_to_offset(EditorState* state) {
    return text_point_to_offset(
        state->document,
        (u64)state->cursor_row,
        (u64)state->cursor_column
    );
}

// Sync cursor (row, col) from a logical byte offset.
internal void cursor_from_offset(EditorState* state, u64 offset) {
    TextPoint pt = text_offset_to_point(state->document, offset);
    state->cursor_row = (i32)pt.line;
    state->cursor_column = (i32)pt.col;
}

internal void set_cursor_from_offset(EditorState* state, u64 offset) {
    cursor_from_offset(state, offset);
    state->desired_column = state->cursor_column;
}

internal void snap_cursor_to_desired_column(EditorState* state) {
    u64 offset = text_point_to_offset(
        state->document,
        (u64)state->cursor_row,
        (u64)state->desired_column
    );
    cursor_from_offset(state, offset);
}

internal void move_cursor(EditorState* state, EditorInput* input) {
    assert(state != nullptr, "Editor state must not be null!");
    assert(input != nullptr, "Editor input must not be null!");

    u64 line_count = text_line_count(state->document);
    i32 max_rows = line_count > 0 ? (i32)(line_count - 1) : 0;

    // Handle char input: insert UTF-8 encoded codepoints
    for(u32 i = 0; i < input->char_input_count; ++i) {
        u32 cp = input->char_inputs[i];
        u8 utf8[4];
        u32 utf8_len = 0;
        if(cp < 0x80) {
            utf8[utf8_len++] = (u8)cp;
        } else if(cp < 0x800) {
            utf8[utf8_len++] = (u8)(0xC0 | (cp >> 6));
            utf8[utf8_len++] = (u8)(0x80 | (cp & 0x3F));
        } else if(cp < 0x10000) {
            utf8[utf8_len++] = (u8)(0xE0 | (cp >> 12));
            utf8[utf8_len++] = (u8)(0x80 | ((cp >> 6) & 0x3F));
            utf8[utf8_len++] = (u8)(0x80 | (cp & 0x3F));
        } else {
            utf8[utf8_len++] = (u8)(0xF0 | (cp >> 18));
            utf8[utf8_len++] = (u8)(0x80 | ((cp >> 12) & 0x3F));
            utf8[utf8_len++] = (u8)(0x80 | ((cp >> 6) & 0x3F));
            utf8[utf8_len++] = (u8)(0x80 | (cp & 0x3F));
        }
        u64 offset = cursor_to_offset(state);
        text_insert(state->document, offset, utf8, utf8_len);
        set_cursor_from_offset(state, offset + utf8_len);
        state->dirty = true;
    }

    bool moved = false;
    for(u32 event_index = 0; event_index < input->key_event_count;
        ++event_index) {
        KeyEvent* event = input->key_events + event_index;
        switch(event->key) {
            case GLFW_KEY_BACKSPACE: {
                u64 offset = cursor_to_offset(state);
                if(offset > 0) {
                    u64 del_offset =
                        text_prev_char_boundary(state->document, offset);
                    u64 del_size = offset - del_offset;
                    text_delete(state->document, del_offset, del_size);
                    set_cursor_from_offset(state, del_offset);
                    state->dirty = true;
                }
            } break;

            case GLFW_KEY_ENTER: {
                u64 offset = cursor_to_offset(state);
                u8 newline = '\n';
                text_insert(state->document, offset, &newline, 1);
                set_cursor_from_offset(state, offset + 1);
                state->dirty = true;
            } break;

            case GLFW_KEY_LEFT: {
                u64 offset = cursor_to_offset(state);
                if(offset > 0)
                    set_cursor_from_offset(
                        state,
                        text_prev_char_boundary(state->document, offset)
                    );
                moved = true;
            } break;

            case GLFW_KEY_RIGHT: {
                u64 offset = cursor_to_offset(state);
                if(offset < text_content_size(state->document))
                    set_cursor_from_offset(
                        state,
                        text_next_char_boundary(state->document, offset)
                    );
                moved = true;
            } break;

            case GLFW_KEY_UP: {
                if(state->cursor_row > 0) {
                    --state->cursor_row;
                    snap_cursor_to_desired_column(state);
                }
                moved = true;
            } break;

            case GLFW_KEY_DOWN: {
                if(state->cursor_row < max_rows) {
                    ++state->cursor_row;
                    snap_cursor_to_desired_column(state);
                }
                moved = true;
            } break;
        }
    }

    line_count = text_line_count(state->document);
    max_rows = line_count > 0 ? (i32)(line_count - 1) : 0;
    i32 clamped_row = clamp(state->cursor_row, 0, max_rows);
    if(clamped_row != state->cursor_row) {
        state->cursor_row = clamped_row;
        snap_cursor_to_desired_column(state);
    }
    state->cursor_column = clamp(state->cursor_column, 0, 4096);
    state->desired_column = clamp(state->desired_column, 0, 4096);

    if(moved || input->char_input_count > 0 || input->scroll_delta != 0.0f) {
        state->blink_timer = 0.0f;
        state->dirty = true;
    }
}

internal void push_cursor(
    EditorState* state,
    EditorInput* input,
    PushCmdBuffer* cmds
) {
    assert(state != nullptr, "Editor state must not be null!");
    assert(input != nullptr, "Editor input must not be null!");
    assert(cmds != nullptr, "Push command buffer must not be null!");

    f32 const blink_period = 1.0f;
    f32 const cursor_width = 3.0f;
    f32 const cursor_height = 22.0f;
    f32 const margin_left = 96.0f;
    f32 const margin_top = 88.0f;
    f32 const cell_width = 14.0f;
    f32 const cell_height = 28.0f;

    state->blink_timer += input->dt_for_frame;
    while(state->blink_timer >= blink_period) {
        state->blink_timer -= blink_period;
    }

    bool cursor_visible = state->blink_timer < (blink_period * 0.5f);
    if(!cursor_visible) {
        return;
    }

    vec2 position = vec2(
        margin_left + state->cursor_column * cell_width,
        margin_top + state->cursor_row * cell_height
    );
    vec2 center = vec2(
        position.x + 0.5f * cursor_width,
        position.y + 0.5f * cursor_height
    );

    push_rect(
        cmds,
        center,
        vec2(cursor_width, cursor_height),
        vec4(0.94f, 0.96f, 0.99f, 1.0f)
    );
}

internal void editor_update_and_render(
    EditorState* state,
    EditorInput* input,
    PushCmdBuffer* cmds
) {
    assert(state != nullptr, "Editor state must not be null!");
    assert(input != nullptr, "Editor input must not be null!");
    assert(cmds != nullptr, "Push command buffer must not be null!");

    push_cmd_buffer_reset(cmds);
    push_clear(cmds, vec4(0.04f, 0.05f, 0.08f, 1.0f));

    move_cursor(state, input);
    push_cursor(state, input, cmds);
}
