#pragma once

// 1 Terabyte of virtual address space reservation.
// Costs nothing in physical RAM until committed.
#define ARENA_RESERVE_SIZE (1 * TB)

inline u64 arenaAlignUp(u64 value, u64 alignment) {
    assert(alignment != 0, "Arena alignment must be non-zero!");
    assert(isPow2(alignment), "Arena alignment must be a power of two!");

    u64 result = 0;
    bool overflow = u64AlignUpPow2(value, alignment, &result);
    assert(!overflow, "Arena alignment overflow!");
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
        u64 page_size = pmGetPageSize();
        a.capacity = ARENA_RESERVE_SIZE;
        a.chunk_size = arenaAlignUp(
            chunk_size != 0u ? chunk_size : page_size,
            page_size
        );
        a.base = (u8*)pmReserveMemory(a.capacity);

        if (a.chunk_size > 0) {
            // Commit one growth chunk up front to avoid first-use OS spikes.
            a.allocateAndCommit(a.chunk_size);
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
        assert(
            required_alignment &&
                !(required_alignment & (required_alignment - 1)),
            "Arena push alignment must be a power of two!"
        );

        u64 size = 0;
        bool size_overflow = u64MulOverflow(sizeof(T), count, &size);
        assert(!size_overflow, "Arena push size overflow!");

        return (T*)pushBytes(size, required_alignment);
    }

    void* pushBytes(u64 size, u64 alignment) {
        assert(alignment != 0, "Arena push alignment must be non-zero!");
        assert(isPow2(alignment), "Arena push alignment must be a power of two!");

        u64 current_ptr = (u64)(base + pos);
        u64 padding =
            (alignment - (current_ptr & (alignment - 1))) &
            (alignment - 1);

        u64 total_size = 0;
        bool total_overflow = u64AddOverflow(size, padding, &total_size);
        assert(!total_overflow, "Arena push size overflow!");

        u8* result_with_padding = (u8*)allocateAndCommit(total_size);
        void* aligned_result = result_with_padding + padding;
        memset(aligned_result, 0, size);
        return aligned_result;
    }

    // Internal: Moves the pointer and handles OS commits
    void* allocateAndCommit(u64 size) {
        u64 new_pos = 0;
        bool pos_overflow = u64AddOverflow(pos, size, &new_pos);
        assert(!pos_overflow, "Arena position overflow!");
        assert(new_pos <= capacity, "Virtual Address Space Exhausted!");

        if (new_pos > committed) {
            u64 needed = new_pos - committed;
            u64 commit_size = arenaAlignUp(needed, chunk_size);

            pmCommitMemory(base + committed, commit_size);
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
        assert(old_pos <= pos, "Arena restore position is out of range!");
        pos = old_pos;
    }

    void clear() {
        pos = 0;
    }

    void release() {
        if (base != nullptr && capacity != 0) {
            pmReleaseMemory(base, capacity);
        }
        *this = {};
    }
};
