#pragma once

#include <cstring>

#include "base/core.h"
#include "base/memory.h"

// 1 Terabyte of virtual address space reservation.
// Costs nothing in physical RAM until committed.
#define ARENA_RESERVE_SIZE (1 * TB)

struct Arena {
    u8 *base;
    u64 capacity;
    u64 committed;
    u64 chunk_size;
    u64 pos;
    i32 temp_count;
};

struct TemporaryMemory {
    Arena *arena;
    u64 pos;
};

#define push_struct(arena, type)                                               \
    (type *)push_size_(arena, sizeof(type), alignof(type))
#define push_array(arena, count, type)                                         \
    (type *)push_size_(arena, (count) * sizeof(type), alignof(type))
#define push_size(arena, size) push_size_(arena, size, 4)

internal u64 align_up(u64 value, u64 alignment) {
    assert(alignment != 0, "Arena alignment must be non-zero!");
    assert(is_pow2(alignment), "Arena alignment must be a power of two!");

    u64 result = 0;
    bool overflow = align_up_pow2_u64(value, alignment, &result);
    assert(!overflow, "Arena alignment overflow!");
    return result;
}

internal void *allocate_and_commit(Arena *arena, u64 size) {
    assert(arena != nullptr, "Arena must not be null!");

    u64 new_pos = 0;
    bool overflow = add_u64_overflow(arena->pos, size, &new_pos);
    assert(!overflow, "Arena position overflow!");
    assert(new_pos <= arena->capacity, "Virtual address space exhausted!");

    if(new_pos > arena->committed) {
        u64 needed = new_pos - arena->committed;
        u64 commit_size = align_up(needed, arena->chunk_size);
        commit_system_memory(arena->base + arena->committed, commit_size);
        arena->committed += commit_size;
    }

    void *result = arena->base + arena->pos;
    arena->pos = new_pos;
    return result;
}

internal void *push_size_(Arena *arena, u64 size, u64 alignment) {
    assert(arena != nullptr, "Arena must not be null!");
    assert(alignment != 0, "Arena push alignment must be non-zero!");
    assert(is_pow2(alignment), "Arena push alignment must be a power of two!");

    u64 current_ptr = (u64)(arena->base + arena->pos);
    u64 padding =
        (alignment - (current_ptr & (alignment - 1))) & (alignment - 1);

    u64 total_size = 0;
    bool overflow = add_u64_overflow(size, padding, &total_size);
    assert(!overflow, "Arena push size overflow!");

    u8 *result_with_padding = (u8 *)allocate_and_commit(arena, total_size);
    void *result = result_with_padding + padding;
    memset(result, 0, size);
    return result;
}

internal TemporaryMemory begin_temporary_memory(Arena *arena) {
    assert(arena != nullptr, "Arena must not be null!");

    TemporaryMemory result = {};
    result.arena = arena;
    result.pos = arena->pos;
    ++arena->temp_count;
    return result;
}

internal void end_temporary_memory(TemporaryMemory temporary_memory) {
    Arena *arena = temporary_memory.arena;
    assert(arena != nullptr, "Temporary arena must not be null!");
    assert(
        arena->pos >= temporary_memory.pos,
        "Arena restore position is out of range!"
    );
    assert(arena->temp_count > 0, "Arena temporary memory count underflow!");

    arena->pos = temporary_memory.pos;
    --arena->temp_count;
}

internal void clear_arena(Arena *arena) {
    assert(arena != nullptr, "Arena must not be null!");
    assert(
        arena->temp_count == 0,
        "Cannot clear arena with outstanding temporary memory!"
    );
    arena->pos = 0;
}

internal void release_arena(Arena *arena) {
    assert(arena != nullptr, "Arena must not be null!");

    if(arena->base != nullptr && arena->capacity != 0) {
        release_system_memory(arena->base, arena->capacity);
    }

    *arena = {};
}

internal Arena create_arena(u64 chunk_size = 4 * KB) {
    Arena arena = {};
    u64 page_size = get_system_page_size();

    arena.capacity = ARENA_RESERVE_SIZE;
    arena.chunk_size =
        align_up(chunk_size != 0 ? chunk_size : page_size, page_size);
    arena.base = (u8 *)reserve_system_memory(arena.capacity);

    if(arena.chunk_size > 0) {
        allocate_and_commit(&arena, arena.chunk_size);
        arena.pos = 0;
    }

    return arena;
}
