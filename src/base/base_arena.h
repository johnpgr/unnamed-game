#pragma once

#include "base/base_core.h"
#include "base/base_list.h"

// Arena header lives in the first 128 bytes of each reserved block.
// The rest of the block is usable memory.
#define ARENA_HEADER_SIZE 128
#define ARENA_DEFAULT_RESERVE (64 * MB)
#define ARENA_DEFAULT_COMMIT (64 * KB)

struct Arena {
    Arena* prev;    // previous block in chain (null on head block)
    Arena* current; // points to the newest (current) block (only valid on head)
    u64 res_size;   // reserve size for new blocks
    u64 cmt_size;   // commit granule for new blocks
    u64 base_pos;   // byte offset of this block's start in the global chain
    u64 pos;        // current write position within this block
    u64 cmt;        // committed bytes in this block
    u64 res;        // reserved bytes in this block
};
static_assert(
    sizeof(Arena) <= ARENA_HEADER_SIZE,
    "Arena header exceeds ARENA_HEADER_SIZE"
);

struct Temp {
    Arena* arena;
    u64 pos;
};

Arena* arena_alloc(
    u64 res_size = ARENA_DEFAULT_RESERVE,
    u64 cmt_size = ARENA_DEFAULT_COMMIT
);
void arena_release(Arena* arena);
void* arena_push(Arena* arena, u64 size, u64 align, bool zero);
u64 arena_pos(Arena* arena);
void arena_pop_to(Arena* arena, u64 pos);
void arena_clear(Arena* arena);
Temp temp_begin(Arena* arena);
void temp_end(Temp t);

// --- push helpers ---

#define push_struct(arena, T) (T*)arena_push((arena), sizeof(T), alignof(T), 1)
#define push_array(arena, T, count)                                            \
    (T*)arena_push((arena), sizeof(T) * (count), alignof(T), 1)
#define push_array_no_zero(arena, T, count)                                    \
    (T*)arena_push((arena), sizeof(T) * (count), alignof(T), 0)
#define push_size(arena, size) arena_push((arena), (size), 8, 1)
