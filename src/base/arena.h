#pragma once

#include <cstring>

#include "base/core.h"
#include "base/memory.h"

// 1 Terabyte of virtual address space reservation.
// Costs nothing in physical RAM until committed.
#define ARENA_RESERVE_SIZE (1 * TB)

inline u64 AlignUp(u64 value, u64 alignment) {
    assert(alignment != 0, "Arena alignment must be non-zero!");
    assert(IsPow2(alignment), "Arena alignment must be a power of two!");

    u64 result = 0;
    bool overflow = AlignUpPow2U64(value, alignment, &result);
    assert(!overflow, "Arena alignment overflow!");
    return result;
}

struct Arena {
    u8* base;
    u64 capacity;  // Total Virtual Address Space
    u64 committed; // Total Physical RAM Backed
    u64 chunk_size;
    u64 pos; // Current "Bump" position

    // --- The Master Generic Push ---
    // Usage: my_arena.push<Entity>();
    // Usage: my_arena.push<Vertex>(100);
    // Usage: my_arena.push<float4>(1, 16); // 16-byte alignment for SIMD
    template <typename T> T* push(u64 count = 1, u64 alignment = alignof(T)) {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "Arena-backed storage only supports trivial, trivially copyable types"
        );

        u64 required_alignment =
            alignment > alignof(T) ? alignment : (u64)alignof(T);
        assert(
            required_alignment &&
                !(required_alignment & (required_alignment - 1)),
            "Arena push alignment must be a power of two!"
        );

        u64 size = 0;
        bool size_overflow = MulU64Overflow(sizeof(T), count, &size);
        assert(!size_overflow, "Arena push size overflow!");

        return (T*)pushBytes(size, required_alignment);
    }

    void* pushBytes(u64 size, u64 alignment) {
        assert(alignment != 0, "Arena push alignment must be non-zero!");
        assert(IsPow2(alignment), "Arena push alignment must be a power of two!");

        u64 current_ptr = (u64)(base + pos);
        u64 padding =
            (alignment - (current_ptr & (alignment - 1))) &
            (alignment - 1);

        u64 total_size = 0;
        bool total_overflow = AddU64Overflow(size, padding, &total_size);
        assert(!total_overflow, "Arena push size overflow!");

        u8* result_with_padding = (u8*)allocateAndCommit(total_size);
        void* aligned_result = result_with_padding + padding;
        memset(aligned_result, 0, size);
        return aligned_result;
    }

    // Internal: Moves the pointer and handles OS commits
    void* allocateAndCommit(u64 size) {
        u64 new_pos = 0;
        bool pos_overflow = AddU64Overflow(pos, size, &new_pos);
        assert(!pos_overflow, "Arena position overflow!");
        assert(new_pos <= capacity, "Virtual Address Space Exhausted!");

        if (new_pos > committed) {
            u64 needed = new_pos - committed;
            u64 commit_size = AlignUp(needed, chunk_size);

            CommitSystemMemory(base + committed, commit_size);
            committed += commit_size;
        }

        void* result = base + pos;
        pos = new_pos;
        return result;
    }

    // Save and restore checkpoints for temporary allocations.
    u64 mark() {
        return pos;
    }

    void restore(u64 old_pos) {
        assert(old_pos <= pos, "Arena restore position is out of range!");
        pos = old_pos;
    }

    void clear() {
        pos = 0;
    }

    void release() {
        if (base != nullptr && capacity != 0) {
            ReleaseSystemMemory(base, capacity);
        }
        *this = {};
    }

};

inline Arena CreateArena(u64 chunk_size = 4 * KB) {
    Arena arena = {};
    u64 page_size = GetSystemPageSize();
    arena.capacity = ARENA_RESERVE_SIZE;
    arena.chunk_size = AlignUp(
        chunk_size != 0u ? chunk_size : page_size,
        page_size
    );
    arena.base = (u8*)ReserveSystemMemory(arena.capacity);

    if (arena.chunk_size > 0) {
        // Commit one growth chunk up front to avoid first-use OS spikes.
        arena.allocateAndCommit(arena.chunk_size);
        arena.pos = 0; // Reset pos as we just "warmed up" the memory
    }

    return arena;
}
