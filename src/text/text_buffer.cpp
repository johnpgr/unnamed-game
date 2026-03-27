#include "text/text_buffer.h"

#include <string.h>

// ---- constants ----

#define TEXT_CHUNK_MAX 128 // max bytes per chunk
#define TEXT_TREE_BASE 6
#define TEXT_TREE_CAP (TEXT_TREE_BASE * 2) // max items per node/leaf
#define TEXT_MAX_DEPTH 24 // max tree height (log_6(file_bytes) ceiling)

// ---- summary (the monoid that aggregates up the tree) ----

struct TextSummary {
    u64 bytes;
    u64 lines; // newline count
    u64 codepoints;
    u64 tail_codepoints; // codepoints after the last newline
    u64 utf16_units;
};

internal TextSummary text_summary_add(TextSummary a, TextSummary b) {
    TextSummary result = {};
    result.bytes = a.bytes + b.bytes;
    result.lines = a.lines + b.lines;
    result.codepoints = a.codepoints + b.codepoints;
    result.tail_codepoints =
        (b.lines > 0) ? b.tail_codepoints : (a.tail_codepoints + b.codepoints);
    result.utf16_units = a.utf16_units + b.utf16_units;
    return result;
}

internal TextSummary text_summary_from_bytes(u8 const* data, u64 len) {
    TextSummary s = {};
    s.bytes = len;
    for(u64 i = 0; i < len; ++i) {
        u8 b = data[i];
        // Count UTF-16 units: codepoints >= U+10000 need 2 units (4-byte UTF-8
        // start: 0xF0-0xFF)
        if((b & 0xC0) != 0x80) { // not a continuation byte → start of codepoint
            ++s.codepoints;
            ++s.utf16_units;
            if(b >= 0xF0)
                ++s.utf16_units; // supplementary plane
            if(b == '\n') {
                ++s.lines;
                s.tail_codepoints = 0;
            } else {
                ++s.tail_codepoints;
            }
        }
    }
    return s;
}

// ---- chunk ----

struct TextChunk {
    u8 text[TEXT_CHUNK_MAX];
    u16 len;
    TextSummary summary;
};

// ---- leaf node (holds chunks, doubly-linked for sequential scan) ----

struct TextLeaf {
    TextLeaf* prev;
    TextLeaf* next;
    u16 count;
    u32 ref_count; // COW: 1 = exclusively owned, >1 = shared with snapshot(s)
    TextSummary summary;
    TextChunk chunks[TEXT_TREE_CAP];
};

// ---- internal node ----

struct TextNode {
    TextNode* next; // used only when node is in free list
    u16 count;
    u8 height;     // 1 = children are leaves, >1 = nodes
    u32 ref_count; // COW: 1 = exclusively owned, >1 = shared with snapshot(s)
    TextSummary summary;
    TextSummary child_summaries[TEXT_TREE_CAP];
    union {
        TextNode* nodes[TEXT_TREE_CAP];
        TextLeaf* leaves[TEXT_TREE_CAP];
    };
};

// ---- document ----

#define TEXT_ANCHOR_MAX 64

struct TextAnchor {
    u64 offset;
    TextAnchorBias bias;
    bool active;
};

// Immutable snapshot of tree state for background readers (Tree-sitter, undo).
// Read via text_snapshot_read (tree descent only — linked list may be stale).
struct TextSnapshot {
    TextNode* root;
    TextLeaf* first_leaf; // for single-leaf docs (no root)
    TextSummary total;
    u64 version;
};

// One undo/redo record: captures tree state before the edit.
struct TextEdit {
    u64 offset;
    u64 old_len;
    u64 new_len;
    TextSnapshot before;
};

#define TEXT_UNDO_MAX 256

struct TextDocument {
    Arena* arena;
    TextNode* root; // null when doc fits in one leaf
    TextLeaf* first_leaf;
    TextLeaf* last_leaf;
    TextSummary total;
    // Free list recycling (raddebugger pattern)
    TextNode* free_nodes;
    TextLeaf* free_leaves;
    // Anchors: positions that survive edits (cursor, selections, bookmarks)
    TextAnchor anchors[TEXT_ANCHOR_MAX];
    u32 anchor_count;
    // Version counter: incremented on every insert/delete
    u64 version;
    // Snapshot reference count: >0 means COW is active
    u32 snapshot_count;
    // Undo stack (snapshot-per-edit, circular buffer)
    TextEdit undo_stack[TEXT_UNDO_MAX];
    u32 undo_head;  // next slot to write
    u32 undo_count; // valid entries (saturates at TEXT_UNDO_MAX)
};

// ---- forward declarations ----

struct TextNodeSplit {
    bool did_split;
    TextNode* right;
    TextSummary right_summary;
};
struct TextLeafSplit {
    bool did_split;
    TextLeaf* right;
    TextSummary right_summary;
};

// ---- internal helpers ----

internal TextLeaf* text_alloc_leaf(TextDocument* doc) {
    TextLeaf* leaf = doc->free_leaves;
    if(leaf) {
        SLL_STACK_POP(doc->free_leaves);
        memset(leaf, 0, sizeof(*leaf));
    } else {
        leaf = push_struct(doc->arena, TextLeaf);
    }
    leaf->ref_count = 1;
    return leaf;
}

internal TextNode* text_alloc_node(TextDocument* doc) {
    TextNode* node = doc->free_nodes;
    if(node) {
        SLL_STACK_POP(doc->free_nodes);
        memset(node, 0, sizeof(*node));
    } else {
        node = push_struct(doc->arena, TextNode);
    }
    node->ref_count = 1;
    return node;
}

internal void text_free_leaf(TextDocument* doc, TextLeaf* leaf) {
    SLL_STACK_PUSH(doc->free_leaves, leaf);
}

internal void text_free_node(TextDocument* doc, TextNode* node) {
    SLL_STACK_PUSH(doc->free_nodes, node);
}

internal bool utf8_is_continuation(u8 byte) {
    return (byte & 0xC0) == 0x80;
}

// Find the last valid UTF-8 codepoint start at or before pos in buf.
internal u16 utf8_boundary_at_or_before(u8 const* buf, u16 pos) {
    while(pos > 0 && utf8_is_continuation(buf[pos]))
        --pos;
    return pos;
}

internal u16 text_chunk_size_for_bytes(u8 const* bytes, u64 len) {
    if(len <= TEXT_CHUNK_MAX)
        return (u16)len;

    u16 chunk_size = utf8_boundary_at_or_before(bytes, TEXT_CHUNK_MAX);
    if(chunk_size == 0)
        chunk_size = TEXT_CHUNK_MAX;
    return chunk_size;
}

internal u16 text_partition_bytes_into_chunks(
    u8 const* bytes,
    u64 len,
    u16* chunk_sizes,
    u16 max_chunks
) {
    u16 chunk_count = 0;
    u64 remaining = len;

    while(remaining > 0) {
        ASSERT(
            chunk_count < max_chunks,
            "text chunk partition overflowed fixed storage"
        );
        u16 chunk_size = text_chunk_size_for_bytes(bytes, remaining);
        ASSERT(chunk_size > 0, "text chunk partition made no forward progress");
        chunk_sizes[chunk_count++] = chunk_size;
        bytes += chunk_size;
        remaining -= chunk_size;
    }

    return chunk_count;
}

internal void text_write_leaf_chunks(
    TextLeaf* leaf,
    u8 const* bytes,
    u16 const* chunk_sizes,
    u16 chunk_count
) {
    ASSERT(
        chunk_count <= TEXT_TREE_CAP,
        "leaf rewrite exceeded maximum chunk capacity"
    );

    leaf->count = chunk_count;
    leaf->summary = {};

    u64 offset = 0;
    for(u16 chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        TextChunk* chunk = &leaf->chunks[chunk_index];
        u16 chunk_size = chunk_sizes[chunk_index];
        chunk->len = chunk_size;
        memcpy(chunk->text, bytes + offset, chunk_size);
        chunk->summary = text_summary_from_bytes(bytes + offset, chunk_size);
        leaf->summary = text_summary_add(leaf->summary, chunk->summary);
        offset += chunk_size;
    }
}

internal TextSummary text_document_summary(TextDocument* doc) {
    if(doc->root)
        return doc->root->summary;
    if(doc->first_leaf)
        return doc->first_leaf->summary;
    return {};
}

internal void text_sync_total(TextDocument* doc) {
    doc->total = text_document_summary(doc);
}

internal void text_point_advance_by_summary(
    TextPoint* pt,
    TextSummary summary
) {
    pt->line += summary.lines;
    if(summary.lines > 0)
        pt->col = summary.tail_codepoints;
    else
        pt->col += summary.codepoints;
}

// Recompute leaf summary from its chunks.
internal void leaf_recompute_summary(TextLeaf* leaf) {
    leaf->summary = {};
    for(int i = 0; i < leaf->count; ++i) {
        leaf->summary =
            text_summary_add(leaf->summary, leaf->chunks[i].summary);
    }
}

// Recompute node summary from child_summaries.
internal void node_recompute_summary(TextNode* node) {
    node->summary = {};
    for(int i = 0; i < node->count; ++i) {
        node->summary =
            text_summary_add(node->summary, node->child_summaries[i]);
    }
}

// Recursively recompute all node summaries from leaves up.
internal void node_recompute_all(TextNode* node) {
    if(node->height > 1) {
        for(int i = 0; i < node->count; ++i)
            node_recompute_all(node->nodes[i]);
    }
    node_recompute_summary(node);
}

// ---- anchor API ----

u32 text_anchor_create(TextDocument* doc, u64 offset, TextAnchorBias bias) {
    for(u32 i = 0; i < TEXT_ANCHOR_MAX; ++i) {
        if(!doc->anchors[i].active) {
            doc->anchors[i] = {offset, bias, true};
            if(i >= doc->anchor_count)
                doc->anchor_count = i + 1;
            return i;
        }
    }
    ASSERT(false, "text_anchor_create: too many anchors");
    return 0;
}

void text_anchor_destroy(TextDocument* doc, u32 id) {
    ASSERT(
        id < TEXT_ANCHOR_MAX && doc->anchors[id].active,
        "invalid anchor id"
    );
    doc->anchors[id].active = false;
}

u64 text_anchor_offset(TextDocument* doc, u32 id) {
    ASSERT(
        id < TEXT_ANCHOR_MAX && doc->anchors[id].active,
        "invalid anchor id"
    );
    return doc->anchors[id].offset;
}

void text_anchor_set(TextDocument* doc, u32 id, u64 offset) {
    ASSERT(
        id < TEXT_ANCHOR_MAX && doc->anchors[id].active,
        "invalid anchor id"
    );
    doc->anchors[id].offset = offset;
}

internal void text_anchors_adjust_insert(
    TextDocument* doc,
    u64 offset,
    u64 len
) {
    for(u32 i = 0; i < doc->anchor_count; ++i) {
        if(!doc->anchors[i].active)
            continue;
        u64 a = doc->anchors[i].offset;
        if(a > offset ||
           (a == offset && doc->anchors[i].bias == TEXT_ANCHOR_RIGHT))
            doc->anchors[i].offset += len;
    }
}

internal void text_anchors_adjust_delete(
    TextDocument* doc,
    u64 offset,
    u64 len
) {
    for(u32 i = 0; i < doc->anchor_count; ++i) {
        if(!doc->anchors[i].active)
            continue;
        u64 a = doc->anchors[i].offset;
        if(a >= offset + len)
            doc->anchors[i].offset -= len;
        else if(a > offset)
            doc->anchors[i].offset = offset;
    }
}

// ---- COW helpers ----

// Clone a node/leaf if it's shared (ref_count > 1). Returns the writable copy.
internal TextNode* maybe_cow_node(TextDocument* doc, TextNode* node) {
    if(node->ref_count <= 1)
        return node;
    TextNode* cow = text_alloc_node(doc);
    *cow = *node;
    cow->ref_count = 1;
    --node->ref_count;
    return cow;
}

internal TextLeaf* maybe_cow_leaf(TextDocument* doc, TextLeaf* leaf) {
    if(leaf->ref_count <= 1) // If not shared
        return leaf;         // Fast path: no need to clone

    TextLeaf* cow = text_alloc_leaf(doc);
    *cow = *leaf;
    cow->ref_count = 1;
    --leaf->ref_count;

    // Fix live-tree linked list so forward iteration stays correct
    if(cow->next)
        cow->next->prev = cow;
    else
        doc->last_leaf = cow;
    if(cow->prev)
        cow->prev->next = cow;
    else
        doc->first_leaf = cow;
    return cow;
}

// ---- leaf insert ----
// Inserts at most TEXT_CHUNK_MAX bytes at local_offset within leaf.
// Returns a TextLeafSplit if the leaf needed to split.
internal TextLeafSplit leaf_insert_small(
    TextDocument* doc,
    TextLeaf* leaf,
    u64 local_offset, // byte offset within this leaf
    u8 const* bytes,
    u16 len // len <= TEXT_CHUNK_MAX
) {
    // Flatten leaf content + new bytes into a stack buffer
    u64 old_size = leaf->summary.bytes;
    u64 new_size = old_size + len;
    // max new_size: TEXT_TREE_CAP * TEXT_CHUNK_MAX + TEXT_CHUNK_MAX = 13 * 128
    // = 1664
    u8 buf[TEXT_TREE_CAP * TEXT_CHUNK_MAX + TEXT_CHUNK_MAX];

    // Copy leaf content into buf
    u64 written = 0;
    for(int i = 0; i < leaf->count; ++i) {
        memcpy(buf + written, leaf->chunks[i].text, leaf->chunks[i].len);
        written += leaf->chunks[i].len;
    }

    // Insert new bytes at local_offset
    memmove(
        buf + local_offset + len,
        buf + local_offset,
        old_size - local_offset
    );
    memcpy(buf + local_offset, bytes, len);

    u16 chunk_sizes[TEXT_TREE_CAP * 2 + 2];
    u16 chunk_count = text_partition_bytes_into_chunks(
        buf,
        new_size,
        chunk_sizes,
        ARRAY_COUNT(chunk_sizes)
    );
    ASSERT(
        chunk_count <= (TEXT_TREE_CAP * 2),
        "leaf insert overflowed two-leaf chunk capacity"
    );

    TextLeafSplit split = {};
    if(chunk_count <= TEXT_TREE_CAP) {
        text_write_leaf_chunks(leaf, buf, chunk_sizes, chunk_count);
        return split;
    }

    TextLeaf* right_leaf = text_alloc_leaf(doc);
    right_leaf->next = leaf->next;
    right_leaf->prev = leaf;
    if(leaf->next)
        leaf->next->prev = right_leaf;
    else
        doc->last_leaf = right_leaf;
    leaf->next = right_leaf;

    u16 left_count = chunk_count / 2;
    u16 right_count = chunk_count - left_count;
    ASSERT(left_count > 0, "split produced empty left leaf");
    ASSERT(right_count > 0, "split produced empty right leaf");
    ASSERT(left_count <= TEXT_TREE_CAP, "split left leaf overflowed");
    ASSERT(right_count <= TEXT_TREE_CAP, "split right leaf overflowed");

    u64 right_offset = 0;
    for(u16 chunk_index = 0; chunk_index < left_count; ++chunk_index)
        right_offset += chunk_sizes[chunk_index];

    text_write_leaf_chunks(leaf, buf, chunk_sizes, left_count);
    text_write_leaf_chunks(
        right_leaf,
        buf + right_offset,
        chunk_sizes + left_count,
        right_count
    );

    split.did_split = true;
    split.right = right_leaf;
    split.right_summary = right_leaf->summary;
    return split;
}

// ---- node insert ----

internal TextNodeSplit node_insert_small(
    TextDocument* doc,
    TextNode* node,
    u64 offset,
    u8 const* bytes,
    u16 len
);

internal TextNodeSplit
node_split_overflowed(TextDocument* doc, TextNode* node) {
    // Split node: right half goes into a new node
    TextNode* right_node = text_alloc_node(doc);
    right_node->height = node->height;

    int left_count = node->count / 2;
    int right_count = node->count - left_count;
    right_node->count = (u16)right_count;

    for(int i = 0; i < right_count; ++i) {
        right_node->child_summaries[i] = node->child_summaries[left_count + i];
        if(node->height == 1)
            right_node->leaves[i] = node->leaves[left_count + i];
        else
            right_node->nodes[i] = node->nodes[left_count + i];
    }
    node->count = (u16)left_count;

    node_recompute_summary(node);
    node_recompute_summary(right_node);

    TextNodeSplit s;
    s.did_split = true;
    s.right = right_node;
    s.right_summary = right_node->summary;
    return s;
}

internal TextNodeSplit node_insert_small(
    TextDocument* doc,
    TextNode* node,
    u64 offset,
    u8 const* bytes,
    u16 len
) {
    // Find which child to descend into
    int child_idx = 0;
    u64 local_offset = offset;
    for(; child_idx < node->count - 1; ++child_idx) {
        if(local_offset < node->child_summaries[child_idx].bytes)
            break;
        local_offset -= node->child_summaries[child_idx].bytes;
    }

    bool did_child_split = false;
    TextNode* right_node = nullptr;
    TextLeaf* right_leaf = nullptr;
    TextSummary right_summary = {};

    if(node->height == 1) {
        node->leaves[child_idx] = maybe_cow_leaf(doc, node->leaves[child_idx]);
        TextLeafSplit s = leaf_insert_small(
            doc,
            node->leaves[child_idx],
            local_offset,
            bytes,
            len
        );
        node->child_summaries[child_idx] = node->leaves[child_idx]->summary;
        if(s.did_split) {
            did_child_split = true;
            right_leaf = s.right;
            right_summary = s.right_summary;
        }
    } else {
        node->nodes[child_idx] = maybe_cow_node(doc, node->nodes[child_idx]);
        TextNodeSplit s = node_insert_small(
            doc,
            node->nodes[child_idx],
            local_offset,
            bytes,
            len
        );
        node->child_summaries[child_idx] = node->nodes[child_idx]->summary;
        if(s.did_split) {
            did_child_split = true;
            right_node = s.right;
            right_summary = s.right_summary;
        }
    }
    node_recompute_summary(node);

    if(!did_child_split)
        return {false};

    // Insert right sibling at child_idx+1
    int insert_at = child_idx + 1;
    // Shift everything right
    for(int i = node->count; i > insert_at; --i) {
        node->child_summaries[i] = node->child_summaries[i - 1];
        if(node->height == 1)
            node->leaves[i] = node->leaves[i - 1];
        else
            node->nodes[i] = node->nodes[i - 1];
    }
    node->child_summaries[insert_at] = right_summary;
    if(node->height == 1)
        node->leaves[insert_at] = right_leaf;
    else
        node->nodes[insert_at] = right_node;
    node->count++;
    node_recompute_summary(node);

    if(node->count <= TEXT_TREE_CAP)
        return {false};

    // Node overflowed — split it
    return node_split_overflowed(doc, node);
}

// ---- public insert ----

void text_insert(TextDocument* doc, u64 byte_offset, u8 const* bytes, u64 len) {
    ASSERT(doc != nullptr, "doc must not be null");
    ASSERT(byte_offset <= doc->total.bytes, "insert offset out of range");

    text_anchors_adjust_insert(doc, byte_offset, len);
    ++doc->version;

    // Segment large inserts so each leaf operation is ≤ TEXT_CHUNK_MAX bytes.
    u64 cursor = byte_offset;
    u8 const* p = bytes;
    u64 remaining = len;

    while(remaining > 0) {
        u16 chunk = text_chunk_size_for_bytes(p, remaining);
        ASSERT(chunk > 0, "text insert made no forward progress");

        if(!doc->first_leaf) {
            doc->first_leaf = doc->last_leaf = text_alloc_leaf(doc);
        }

        if(!doc->root) {
            // COW single leaf if shared
            doc->first_leaf = maybe_cow_leaf(doc, doc->first_leaf);
            TextLeafSplit s =
                leaf_insert_small(doc, doc->first_leaf, cursor, p, chunk);
            if(s.did_split) {
                TextNode* root = text_alloc_node(doc);
                root->height = 1;
                root->count = 2;
                root->leaves[0] = doc->first_leaf;
                root->child_summaries[0] = doc->first_leaf->summary;
                root->leaves[1] = s.right;
                root->child_summaries[1] = s.right_summary;
                node_recompute_summary(root);
                doc->root = root;
            }
        } else {
            doc->root = maybe_cow_node(doc, doc->root);
            TextNodeSplit s =
                node_insert_small(doc, doc->root, cursor, p, chunk);
            if(s.did_split) {
                TextNode* new_root = text_alloc_node(doc);
                new_root->height = doc->root->height + 1;
                new_root->count = 2;
                new_root->nodes[0] = doc->root;
                new_root->child_summaries[0] = doc->root->summary;
                new_root->nodes[1] = s.right;
                new_root->child_summaries[1] = s.right_summary;
                node_recompute_summary(new_root);
                doc->root = new_root;
            }
        }

        cursor += chunk;
        p += chunk;
        remaining -= chunk;
    }

    text_sync_total(doc);
}

// ---- delete helpers ----

// Remove child at index from node, update summaries.
// Returns true if node is now empty.
internal bool node_remove_child(TextNode* node, int idx) {
    for(int i = idx; i < node->count - 1; ++i) {
        node->child_summaries[i] = node->child_summaries[i + 1];
        if(node->height == 1)
            node->leaves[i] = node->leaves[i + 1];
        else
            node->nodes[i] = node->nodes[i + 1];
    }
    node->count--;
    node_recompute_summary(node);
    return node->count == 0;
}

// Descend to find and remove a specific leaf pointer from the tree.
// Returns true if node should be removed (empty).
internal bool node_remove_leaf(
    TextDocument* doc,
    TextNode* node,
    TextLeaf* leaf
) {
    if(node->height == 1) {
        for(int i = 0; i < node->count; ++i) {
            if(node->leaves[i] == leaf) {
                return node_remove_child(node, i);
            }
        }
        return false;
    }
    for(int i = 0; i < node->count; ++i) {
        if(node_remove_leaf(doc, node->nodes[i], leaf)) {
            text_free_node(doc, node->nodes[i]);
            return node_remove_child(node, i);
        }
    }
    return false;
}

internal void node_refresh_summary_for_leaf(
    TextNode* node,
    u64 leaf_start,
    TextLeaf* leaf
) {
    int child_idx = 0;
    u64 local_offset = leaf_start;
    for(; child_idx < node->count - 1; ++child_idx) {
        if(local_offset < node->child_summaries[child_idx].bytes)
            break;
        local_offset -= node->child_summaries[child_idx].bytes;
    }

    if(node->height == 1) {
        ASSERT(node->leaves[child_idx] == leaf, "leaf path refresh mismatch");
        node->child_summaries[child_idx] = leaf->summary;
    } else {
        node_refresh_summary_for_leaf(
            node->nodes[child_idx],
            local_offset,
            leaf
        );
        node->child_summaries[child_idx] = node->nodes[child_idx]->summary;
    }
    node_recompute_summary(node);
}

// ---- find leaf by byte offset (tree descent) ----

struct TextLeafPos {
    TextLeaf* leaf;
    u64 leaf_start;
}; // leaf + offset of leaf's first byte

internal TextLeafPos text_find_leaf(TextDocument* doc, u64 byte_offset) {
    if(!doc->root) {
        return {doc->first_leaf, 0};
    }
    TextNode* node = doc->root;
    u64 accumulated = 0;
    while(node->height > 1) {
        int i = 0;
        for(; i < node->count - 1; ++i) {
            if(byte_offset - accumulated < node->child_summaries[i].bytes)
                break;
            accumulated += node->child_summaries[i].bytes;
        }
        node = node->nodes[i];
    }
    // node is now at height 1, children are leaves
    int i = 0;
    for(; i < node->count - 1; ++i) {
        if(byte_offset - accumulated < node->child_summaries[i].bytes)
            break;
        accumulated += node->child_summaries[i].bytes;
    }
    return {node->leaves[i], accumulated};
}

struct TextByteLocation {
    TextLeaf* leaf;
    u16 chunk_index;
    u16 byte_index;
};

internal TextByteLocation
text_resolve_byte_location(TextDocument* doc, u64 byte_offset) {
    ASSERT(byte_offset < doc->total.bytes, "byte offset out of range");

    TextLeafPos lp = text_find_leaf(doc, byte_offset);
    u64 local_offset = byte_offset - lp.leaf_start;
    u64 accumulated = 0;

    for(int chunk_index = 0; chunk_index < lp.leaf->count; ++chunk_index) {
        TextChunk* chunk = &lp.leaf->chunks[chunk_index];
        if(local_offset < accumulated + chunk->len) {
            return {
                lp.leaf,
                (u16)chunk_index,
                (u16)(local_offset - accumulated)
            };
        }
        accumulated += chunk->len;
    }

    ASSERT(false, "failed to resolve byte within leaf");
    return {};
}

u64 text_prev_char_boundary(TextDocument* doc, u64 byte_offset) {
    if(byte_offset == 0)
        return 0;

    TextByteLocation location =
        text_resolve_byte_location(doc, byte_offset - 1);
    TextChunk* chunk = &location.leaf->chunks[location.chunk_index];

    u64 result = byte_offset - 1;
    while(result > 0 && location.byte_index > 0 &&
          utf8_is_continuation(chunk->text[location.byte_index])) {
        --location.byte_index;
        --result;
    }
    return result;
}

u64 text_next_char_boundary(TextDocument* doc, u64 byte_offset) {
    if(byte_offset >= doc->total.bytes)
        return doc->total.bytes;

    TextByteLocation location = text_resolve_byte_location(doc, byte_offset);
    TextChunk* chunk = &location.leaf->chunks[location.chunk_index];

    u64 result = byte_offset + 1;
    u16 byte_index = location.byte_index + 1;
    while(result < doc->total.bytes && byte_index < chunk->len &&
          utf8_is_continuation(chunk->text[byte_index])) {
        ++byte_index;
        ++result;
    }
    return result;
}

// ---- delete within one leaf ----
// Deletes [local_start, local_start+len) from the leaf.
// Returns true if leaf became empty.
internal bool leaf_delete_range(
    TextLeaf* leaf,
    u64 local_start,
    u64 local_len
) {
    // Flatten leaf content
    u64 old_size = leaf->summary.bytes;
    u8 buf[TEXT_TREE_CAP * TEXT_CHUNK_MAX];
    u64 written = 0;
    for(int i = 0; i < leaf->count; ++i) {
        memcpy(buf + written, leaf->chunks[i].text, leaf->chunks[i].len);
        written += leaf->chunks[i].len;
    }

    // Remove [local_start, local_start+local_len)
    u64 new_size = old_size - local_len;
    memmove(
        buf + local_start,
        buf + local_start + local_len,
        old_size - local_start - local_len
    );

    // Redistribute into chunks
    leaf->count = 0;
    leaf->summary = {};
    u64 rem = new_size;
    u8* p = buf;
    while(rem > 0) {
        u16 clen = text_chunk_size_for_bytes(p, rem);
        TextChunk* chunk = &leaf->chunks[leaf->count++];
        chunk->len = clen;
        memcpy(chunk->text, p, clen);
        chunk->summary = text_summary_from_bytes(p, clen);
        leaf->summary = text_summary_add(leaf->summary, chunk->summary);
        p += clen;
        rem -= clen;
    }
    return leaf->count == 0;
}

// ---- delete rebalancing ----
// Merge leaf with its right neighbor if combined chunks fit in one leaf.
// leaf_start is the byte offset of leaf's first byte (for summary refresh).
// Returns true if merge happened.
internal bool leaf_try_merge_right(
    TextDocument* doc,
    TextLeaf* leaf,
    u64 leaf_start
) {
    if(!leaf->next)
        return false;
    TextLeaf* right = leaf->next;
    if(leaf->count + right->count > TEXT_TREE_CAP)
        return false;

    // Absorb right's chunks
    for(int i = 0; i < right->count; ++i)
        leaf->chunks[leaf->count + i] = right->chunks[i];
    leaf->count += right->count;
    leaf_recompute_summary(leaf);

    // Unlink right from linked list
    leaf->next = right->next;
    if(right->next)
        right->next->prev = leaf;
    else
        doc->last_leaf = leaf;

    // Remove right from tree and refresh ancestor summaries
    if(doc->root) {
        bool root_empty = node_remove_leaf(doc, doc->root, right);
        if(root_empty) {
            text_free_node(doc, doc->root);
            doc->root = nullptr;
        } else if(doc->root->count == 1 && doc->root->height > 1) {
            TextNode* old_root = doc->root;
            doc->root = old_root->nodes[0];
            text_free_node(doc, old_root);
        }
        if(doc->root)
            node_refresh_summary_for_leaf(doc->root, leaf_start, leaf);
    }
    text_free_leaf(doc, right);
    return true;
}

// ---- public delete ----

void text_delete(TextDocument* doc, u64 byte_offset, u64 len) {
    ASSERT(doc != nullptr, "doc must not be null");
    ASSERT(byte_offset + len <= doc->total.bytes, "delete out of range");
    if(len == 0)
        return;

    text_anchors_adjust_delete(doc, byte_offset, len);
    ++doc->version;

    u64 remaining = len;
    while(remaining > 0) {
        TextLeafPos lp = text_find_leaf(doc, byte_offset);
        // COW leaf before mutation
        TextLeaf* leaf = maybe_cow_leaf(doc, lp.leaf);
        lp.leaf = leaf;

        u64 local_start = byte_offset - lp.leaf_start;
        u64 leaf_avail = leaf->summary.bytes - local_start;
        u64 delete_now = remaining < leaf_avail ? remaining : leaf_avail;

        bool leaf_empty = leaf_delete_range(leaf, local_start, delete_now);

        if(leaf_empty) {
            // Remove from linked list
            if(leaf->prev)
                leaf->prev->next = leaf->next;
            else
                doc->first_leaf = leaf->next;
            if(leaf->next)
                leaf->next->prev = leaf->prev;
            else
                doc->last_leaf = leaf->prev;

            if(doc->root) {
                bool root_empty = node_remove_leaf(doc, doc->root, leaf);
                if(root_empty) {
                    text_free_node(doc, doc->root);
                    doc->root = nullptr;
                } else if(doc->root->count == 1 && doc->root->height > 1) {
                    TextNode* old_root = doc->root;
                    doc->root = old_root->nodes[0];
                    text_free_node(doc, old_root);
                }
            }
            text_free_leaf(doc, leaf);
        } else {
            // Refresh summaries, then try merging underfull leaf with neighbor
            if(doc->root)
                node_refresh_summary_for_leaf(doc->root, lp.leaf_start, leaf);
            if(leaf->count < TEXT_TREE_BASE)
                leaf_try_merge_right(doc, leaf, lp.leaf_start);
        }

        remaining -= delete_now;
    }

    text_sync_total(doc);
}

// ---- create ----

TextDocument* text_document_create(Arena* arena, String content) {
    TextDocument* doc = push_struct(arena, TextDocument);
    doc->arena = arena;

    if(content.size == 0) {
        // Empty document still needs one leaf
        doc->first_leaf = doc->last_leaf = text_alloc_leaf(doc);
        return doc;
    }

    text_insert(doc, 0, content.str, content.size);
    return doc;
}

// ---- queries ----

u64 text_content_size(TextDocument* doc) {
    return doc->total.bytes;
}
u64 text_line_count(TextDocument* doc) {
    return doc->total.lines + 1;
}

internal void text_skip_summary(
    TextSummary summary,
    u64* line,
    u64* col,
    u64* offset
) {
    TextPoint pt = {*line, *col};
    text_point_advance_by_summary(&pt, summary);
    *line = pt.line;
    *col = pt.col;
    *offset += summary.bytes;
}

internal bool text_summary_can_skip_for_point(
    TextSummary summary,
    u64 target_line,
    u64 target_col,
    u64 line,
    u64 col
) {
    if(line + summary.lines < target_line)
        return true;
    if(line == target_line && summary.lines == 0 &&
       col + summary.codepoints <= target_col) {
        return true;
    }
    return false;
}

internal bool text_point_to_offset_in_leaf(
    TextLeaf* leaf,
    u64 target_line,
    u64 target_col,
    u64* line,
    u64* col,
    u64* offset
) {
    if(*line == target_line && *col == target_col)
        return true;

    for(int chunk_index = 0; chunk_index < leaf->count; ++chunk_index) {
        TextChunk* chunk = &leaf->chunks[chunk_index];
        if(text_summary_can_skip_for_point(
               chunk->summary,
               target_line,
               target_col,
               *line,
               *col
           )) {
            text_skip_summary(chunk->summary, line, col, offset);
            continue;
        }

        for(u16 byte_index = 0; byte_index < chunk->len; ++byte_index) {
            if(utf8_is_continuation(chunk->text[byte_index])) {
                ++(*offset);
                continue;
            }

            if(*line == target_line) {
                if(*col == target_col)
                    return true;
                if(chunk->text[byte_index] == '\n')
                    return true;
            }

            if(chunk->text[byte_index] == '\n') {
                ++(*line);
                *col = 0;
            } else {
                ++(*col);
            }
            ++(*offset);
        }
    }

    return false;
}

internal bool text_point_to_offset_in_node(
    TextNode* node,
    u64 target_line,
    u64 target_col,
    u64* line,
    u64* col,
    u64* offset
) {
    if(*line == target_line && *col == target_col)
        return true;

    for(int child_index = 0; child_index < node->count; ++child_index) {
        TextSummary summary = node->child_summaries[child_index];
        if(text_summary_can_skip_for_point(
               summary,
               target_line,
               target_col,
               *line,
               *col
           )) {
            text_skip_summary(summary, line, col, offset);
            continue;
        }

        if(node->height == 1) {
            if(text_point_to_offset_in_leaf(
                   node->leaves[child_index],
                   target_line,
                   target_col,
                   line,
                   col,
                   offset
               )) {
                return true;
            }
            continue;
        }
        if(text_point_to_offset_in_node(
               node->nodes[child_index],
               target_line,
               target_col,
               line,
               col,
               offset
           )) {
            return true;
        }
    }

    return false;
}

// Convert logical byte offset → (line, col)
TextPoint text_offset_to_point(TextDocument* doc, u64 byte_offset) {
    u64 remaining =
        byte_offset < doc->total.bytes ? byte_offset : doc->total.bytes;
    TextPoint pt = {};

    if(doc->root) {
        TextNode* node = doc->root;
        while(node->height > 1) {
            int child_index = 0;
            for(; child_index < node->count; ++child_index) {
                TextSummary summary = node->child_summaries[child_index];
                if(remaining < summary.bytes)
                    break;
                text_point_advance_by_summary(&pt, summary);
                remaining -= summary.bytes;
            }
            if(child_index == node->count)
                return pt;
            node = node->nodes[child_index];
        }

        for(int child_index = 0; child_index < node->count; ++child_index) {
            TextLeaf* leaf = node->leaves[child_index];
            if(remaining < leaf->summary.bytes) {
                for(int chunk_index = 0; chunk_index < leaf->count;
                    ++chunk_index) {
                    TextChunk* chunk = &leaf->chunks[chunk_index];
                    if(remaining < chunk->len) {
                        for(u16 byte_index = 0; byte_index < remaining;
                            ++byte_index) {
                            if(!utf8_is_continuation(chunk->text[byte_index])) {
                                if(chunk->text[byte_index] == '\n') {
                                    ++pt.line;
                                    pt.col = 0;
                                } else {
                                    ++pt.col;
                                }
                            }
                        }
                        return pt;
                    }
                    text_point_advance_by_summary(&pt, chunk->summary);
                    remaining -= chunk->len;
                }
                return pt;
            }
            text_point_advance_by_summary(&pt, leaf->summary);
            remaining -= leaf->summary.bytes;
        }
        return pt;
    }

    TextLeaf* leaf = doc->first_leaf;
    if(leaf) {
        for(int chunk_index = 0; chunk_index < leaf->count; ++chunk_index) {
            TextChunk* chunk = &leaf->chunks[chunk_index];
            if(remaining < chunk->len) {
                for(u16 byte_index = 0; byte_index < remaining; ++byte_index) {
                    if(!utf8_is_continuation(chunk->text[byte_index])) {
                        if(chunk->text[byte_index] == '\n') {
                            ++pt.line;
                            pt.col = 0;
                        } else {
                            ++pt.col;
                        }
                    }
                }
                return pt;
            }
            text_point_advance_by_summary(&pt, chunk->summary);
            remaining -= chunk->len;
        }
    }

    return pt;
}

// Convert (line, col) → logical byte offset
u64 text_point_to_offset(TextDocument* doc, u64 line, u64 col) {
    u64 cur_line = 0;
    u64 cur_col = 0;
    u64 offset = 0;

    if(doc->root) {
        text_point_to_offset_in_node(
            doc->root,
            line,
            col,
            &cur_line,
            &cur_col,
            &offset
        );
    } else if(doc->first_leaf) {
        text_point_to_offset_in_leaf(
            doc->first_leaf,
            line,
            col,
            &cur_line,
            &cur_col,
            &offset
        );
    }

    return offset;
}

internal u64 text_line_start_offset(TextDocument* doc, u64 line_idx) {
    return text_point_to_offset(doc, line_idx, 0);
}

internal u64 text_line_end_offset_from(TextDocument* doc, u64 start_offset) {
    if(start_offset >= doc->total.bytes)
        return doc->total.bytes;

    TextByteLocation location = text_resolve_byte_location(doc, start_offset);
    TextLeaf* leaf = location.leaf;
    u64 offset = start_offset;

    for(; leaf; leaf = leaf->next) {
        int chunk_index = (leaf == location.leaf) ? location.chunk_index : 0;
        u16 byte_index = (leaf == location.leaf) ? location.byte_index : 0;
        for(; chunk_index < leaf->count; ++chunk_index) {
            TextChunk* chunk = &leaf->chunks[chunk_index];
            if(chunk->summary.lines == 0) {
                offset += chunk->len - byte_index;
                byte_index = 0;
                continue;
            }

            for(; byte_index < chunk->len; ++byte_index, ++offset) {
                if(chunk->text[byte_index] == '\n')
                    return offset + 1;
            }
            byte_index = 0;
        }
    }

    return offset;
}

internal u64 text_line_end_offset(TextDocument* doc, u64 line_idx) {
    u64 start_offset = text_line_start_offset(doc, line_idx);
    return text_line_end_offset_from(doc, start_offset);
}

// ---- iterator ----
// Zero-copy sequential byte reader. Advances through chunks/leaves via linked
// list. Use text_snapshot_read for snapshot (background thread) reads instead.

struct TextIterator {
    TextDocument* doc;
    TextLeaf* leaf;
    u16 chunk_index;
    u16 byte_index;
    u64 global_offset;
};

internal TextIterator
text_iterator_at_offset(TextDocument* doc, u64 byte_offset) {
    if(!doc->first_leaf || doc->total.bytes == 0)
        return {doc, nullptr, 0, 0, 0};
    if(byte_offset >= doc->total.bytes) {
        TextLeaf* last = doc->last_leaf;
        return {
            doc,
            last,
            last ? (u16)last->count : (u16)0,
            0,
            doc->total.bytes
        };
    }
    TextLeafPos lp = text_find_leaf(doc, byte_offset);
    u64 local = byte_offset - lp.leaf_start;
    u64 acc = 0;
    for(int ci = 0; ci < lp.leaf->count; ++ci) {
        u64 clen = lp.leaf->chunks[ci].len;
        if(local < acc + clen)
            return {doc, lp.leaf, (u16)ci, (u16)(local - acc), byte_offset};
        acc += clen;
    }
    return {doc, lp.leaf, (u16)lp.leaf->count, 0, byte_offset};
}

internal TextIterator text_iterator_at_line(TextDocument* doc, u64 line) {
    return text_iterator_at_offset(doc, text_point_to_offset(doc, line, 0));
}

internal u64 text_iterator_read(TextIterator* iter, u8* buf, u64 max_bytes) {
    u64 written = 0;
    while(written < max_bytes && iter->leaf) {
        if(iter->chunk_index >= iter->leaf->count) {
            iter->leaf = iter->leaf->next;
            iter->chunk_index = 0;
            iter->byte_index = 0;
            if(!iter->leaf)
                break;
        }
        TextChunk* chunk = &iter->leaf->chunks[iter->chunk_index];
        u64 avail = chunk->len - iter->byte_index;
        u64 copy =
            avail < (max_bytes - written) ? avail : (max_bytes - written);
        memcpy(buf + written, chunk->text + iter->byte_index, copy);
        written += copy;
        iter->byte_index += (u16)copy;
        iter->global_offset += copy;
        if(iter->byte_index >= chunk->len) {
            ++iter->chunk_index;
            iter->byte_index = 0;
        }
    }
    return written;
}

internal void text_iterator_advance(TextIterator* iter, u64 n_bytes) {
    u64 rem = n_bytes;
    while(rem > 0 && iter->leaf) {
        if(iter->chunk_index >= iter->leaf->count) {
            iter->leaf = iter->leaf->next;
            iter->chunk_index = 0;
            iter->byte_index = 0;
            if(!iter->leaf)
                break;
        }
        TextChunk* chunk = &iter->leaf->chunks[iter->chunk_index];
        u64 avail = chunk->len - iter->byte_index;
        if(rem < avail) {
            iter->byte_index += (u16)rem;
            iter->global_offset += rem;
            rem = 0;
        } else {
            iter->global_offset += avail;
            rem -= avail;
            ++iter->chunk_index;
            iter->byte_index = 0;
        }
    }
}

// Copy one line's content into scratch arena. Returns a String slice.
internal String
text_line_content(TextDocument* doc, u64 line_idx, Arena* scratch) {
    u64 start_offset = text_line_start_offset(doc, line_idx);
    u64 end_offset = text_line_end_offset_from(doc, start_offset);
    u64 size = end_offset - start_offset;
    if(size == 0)
        return {(u8 const*)"", 0};

    u8* buf = push_array(scratch, u8, size + 1);
    TextIterator iter = text_iterator_at_offset(doc, start_offset);
    u64 written = text_iterator_read(&iter, buf, size);

    if(written > 0 && buf[written - 1] == '\n')
        --written;
    buf[written] = 0;
    return {buf, written};
}

// ---- snapshots ----
// Immutable views used by background threads (Tree-sitter) and the undo stack.
// Reading uses tree descent only — DO NOT walk leaf->next/prev on a snapshot.

internal void text_snapshot_bump_node(TextNode* node) {
    ++node->ref_count;
    if(node->height == 1) {
        for(int i = 0; i < node->count; ++i)
            ++node->leaves[i]->ref_count;
    } else {
        for(int i = 0; i < node->count; ++i)
            text_snapshot_bump_node(node->nodes[i]);
    }
}

internal TextSnapshot text_snapshot(TextDocument* doc) {
    TextSnapshot snap = {};
    snap.root = doc->root;
    snap.first_leaf = doc->first_leaf;
    snap.total = doc->total;
    snap.version = doc->version;
    if(doc->root)
        text_snapshot_bump_node(doc->root);
    else if(doc->first_leaf)
        ++doc->first_leaf->ref_count;
    ++doc->snapshot_count;
    return snap;
}

internal void text_snapshot_release_node(TextDocument* doc, TextNode* node) {
    if(--node->ref_count == 0) {
        if(node->height == 1) {
            for(int i = 0; i < node->count; ++i) {
                if(--node->leaves[i]->ref_count == 0)
                    text_free_leaf(doc, node->leaves[i]);
            }
        } else {
            for(int i = 0; i < node->count; ++i)
                text_snapshot_release_node(doc, node->nodes[i]);
        }
        text_free_node(doc, node);
    }
}

internal void text_snapshot_release(TextDocument* doc, TextSnapshot snap) {
    if(snap.root)
        text_snapshot_release_node(doc, snap.root);
    else if(snap.first_leaf) {
        if(--snap.first_leaf->ref_count == 0)
            text_free_leaf(doc, snap.first_leaf);
    }
    --doc->snapshot_count;
}

// Read bytes from snapshot via tree descent (safe for background threads).
// Returns bytes actually read (0 = past end).
internal u64
text_snapshot_read(TextSnapshot* snap, u64 offset, u8* buf, u64 max_bytes) {
    if(offset >= snap->total.bytes)
        return 0;
    u64 to_read = snap->total.bytes - offset;
    if(to_read > max_bytes)
        to_read = max_bytes;
    u64 written = 0;
    u64 cursor = offset;

    while(written < to_read) {
        // Descent: find leaf and local offset via snapshot's tree
        TextLeaf* leaf;
        u64 leaf_start;
        if(!snap->root) {
            leaf = snap->first_leaf;
            leaf_start = 0;
        } else {
            TextNode* node = snap->root;
            u64 acc = 0;
            while(node->height > 1) {
                int i = 0;
                for(; i < node->count - 1; ++i) {
                    if(cursor - acc < node->child_summaries[i].bytes)
                        break;
                    acc += node->child_summaries[i].bytes;
                }
                node = node->nodes[i];
            }
            int i = 0;
            for(; i < node->count - 1; ++i) {
                if(cursor - acc < node->child_summaries[i].bytes)
                    break;
                acc += node->child_summaries[i].bytes;
            }
            leaf = node->leaves[i];
            leaf_start = acc;
        }

        // Copy bytes from this leaf
        u64 local = cursor - leaf_start;
        u64 acc2 = 0;
        for(int ci = 0; ci < leaf->count && written < to_read; ++ci) {
            TextChunk* chunk = &leaf->chunks[ci];
            if(local < acc2 + chunk->len) {
                u16 start_in = (u16)(local - acc2);
                u64 avail = chunk->len - start_in;
                u64 copy =
                    avail < (to_read - written) ? avail : (to_read - written);
                memcpy(buf + written, chunk->text + start_in, copy);
                written += copy;
                cursor += copy;
                local = 0;
                acc2 = 0;
                continue; // restart inner loop from beginning of next chunk
            }
            acc2 += chunk->len;
        }
        // Guard: if we made no progress (shouldn't happen), break
        if(written == 0 && cursor == offset)
            break;
    }
    return written;
}

// Push an undo entry with a before-snapshot. Call BEFORE mutating the doc.
// Does nothing if at capacity (oldest entry silently dropped).
internal void text_undo_push(
    TextDocument* doc,
    u64 offset,
    u64 old_len,
    u64 new_len
) {
    TextEdit* edit = &doc->undo_stack[doc->undo_head % TEXT_UNDO_MAX];
    // Release old snapshot at this slot if overwriting
    if(doc->undo_count == TEXT_UNDO_MAX)
        text_snapshot_release(doc, edit->before);
    edit->offset = offset;
    edit->old_len = old_len;
    edit->new_len = new_len;
    edit->before = text_snapshot(doc);
    doc->undo_head = (doc->undo_head + 1) % TEXT_UNDO_MAX;
    if(doc->undo_count < TEXT_UNDO_MAX)
        ++doc->undo_count;
}
