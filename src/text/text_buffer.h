#pragma once

#include "base/base_arena.h"
#include "base/base_string.h"

struct TextDocument;

struct TextPoint {
    u64 line;
    u64 col;
};

enum TextAnchorBias : u8 {
    TEXT_ANCHOR_LEFT = 0,
    TEXT_ANCHOR_RIGHT = 1,
};

TextDocument* text_document_create(Arena* arena, String content);
u32 text_anchor_create(
    TextDocument* doc,
    u64 offset,
    TextAnchorBias bias
);
void text_anchor_destroy(TextDocument* doc, u32 id);
u64 text_anchor_offset(TextDocument* doc, u32 id);
void text_anchor_set(TextDocument* doc, u32 id, u64 offset);
void text_insert(
    TextDocument* doc,
    u64 byte_offset,
    u8 const* bytes,
    u64 len
);
void text_delete(TextDocument* doc, u64 byte_offset, u64 len);
u64 text_content_size(TextDocument* doc);
u64 text_line_count(TextDocument* doc);
u64 text_prev_char_boundary(TextDocument* doc, u64 byte_offset);
u64 text_next_char_boundary(TextDocument* doc, u64 byte_offset);
TextPoint text_offset_to_point(TextDocument* doc, u64 byte_offset);
u64 text_point_to_offset(TextDocument* doc, u64 line, u64 col);
