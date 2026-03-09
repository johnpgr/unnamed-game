#pragma once

#include "platform/platform.h"

// 1 Terabyte of virtual address space reservation.
// Costs nothing in physical RAM until committed.
#define ARENA_RESERVE_SIZE (1 * TB)

inline u64 arena_align_up(u64 value, u64 alignment) {
    assert_msg(alignment != 0, "Arena alignment must be non-zero!");
    assert_msg(is_pow2(alignment), "Arena alignment must be a power of two!");

    u64 result = 0;
    bool overflow = u64_align_up_pow2(value, alignment, &result);
    assert_msg(!overflow, "Arena alignment overflow!");
    return result;
}

struct Arena {
    u8* base;
    u64 capacity;  // Total Virtual Address Space
    u64 committed; // Total Physical RAM Backed
    u64 chunk_size;
    u64 pos; // Current "Bump" position

    // --- Core Factory ---
    static Arena make(u64 chunk_size = 4 * KB) {
        Arena a = {};
        u64 page_size = os_memory_page_size();
        a.capacity = ARENA_RESERVE_SIZE;
        a.chunk_size = arena_align_up(
            chunk_size != 0u ? chunk_size : page_size,
            page_size
        );
        a.base = (u8*)os_memory_reserve(a.capacity);

        if (a.chunk_size > 0) {
            // Commit one growth chunk up front to avoid first-use OS spikes.
            a.allocate_and_commit(a.chunk_size);
            a.pos = 0; // Reset pos as we just "warmed up" the memory
        }
        return a;
    }

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
        assert_msg(
            required_alignment &&
                !(required_alignment & (required_alignment - 1)),
            "Arena push alignment must be a power of two!"
        );

        u64 size = 0;
        bool size_overflow = u64_mul_overflow(sizeof(T), count, &size);
        assert_msg(!size_overflow, "Arena push size overflow!");

        // 1. Calculate Alignment Padding
        u64 current_ptr = (u64)(base + pos);
        u64 padding =
            (required_alignment - (current_ptr & (required_alignment - 1))) &
            (required_alignment - 1);

        u64 total_size = 0;
        bool total_overflow = u64_add_overflow(size, padding, &total_size);
        assert_msg(!total_overflow, "Arena push size overflow!");

        // 2. Commit and Advance
        u8* result_with_padding = (u8*)allocate_and_commit(total_size);

        // 3. Offset to the aligned address
        void* aligned_result = result_with_padding + padding;
        T* final_ptr = (T*)aligned_result;

        // 4. Zero-Initialize (Standard Handmade practice)
        memset(final_ptr, 0, size);

        return final_ptr;
    }

    // Internal: Moves the pointer and handles OS commits
    void* allocate_and_commit(u64 size) {
        u64 new_pos = 0;
        bool pos_overflow = u64_add_overflow(pos, size, &new_pos);
        assert_msg(!pos_overflow, "Arena position overflow!");
        assert_msg(new_pos <= capacity, "Virtual Address Space Exhausted!");

        if (new_pos > committed) {
            u64 needed = new_pos - committed;
            u64 commit_size = arena_align_up(needed, chunk_size);

            os_memory_commit(base + committed, commit_size);
            committed += commit_size;
        }

        void* result = base + pos;
        pos = new_pos;
        return result;
    }

    // Save and restore checkpoints for temporary allocations.
    u64 mark() const {
        return pos;
    }

    void restore(u64 old_pos) {
        assert_msg(old_pos <= pos, "Arena restore position is out of range!");
        pos = old_pos;
    }

    void clear() {
        pos = 0;
    }

    void release() {
        if (base != nullptr && capacity != 0) {
            os_memory_release(base, capacity);
        }
        *this = {};
    }
};
