#include <string.h>

#include "base/base_arena.h"
#include "os/os_mod.h"

internal u64 align_up(u64 value, u64 alignment) {
    ASSERT(alignment != 0, "alignment must be non-zero");
    ASSERT(is_pow2(alignment), "alignment must be pow2");
    u64 result = 0;
    bool overflow = align_up_pow2_u64(value, alignment, &result);
    ASSERT(!overflow, "alignment overflow");
    return result;
}

Arena* arena_alloc(u64 res_size, u64 cmt_size) {
    u64 page = get_system_page_size();
    res_size = align_up(res_size, page);
    cmt_size = align_up(cmt_size, page);
    if(cmt_size > res_size) {
        cmt_size = res_size;
    }

    void* mem = reserve_system_memory(res_size);
    commit_system_memory(mem, cmt_size);

    Arena* arena = (Arena*)mem;
    arena->prev = nullptr;
    arena->current = arena;
    arena->res_size = res_size;
    arena->cmt_size = cmt_size;
    arena->base_pos = 0;
    arena->pos = ARENA_HEADER_SIZE;
    arena->cmt = cmt_size;
    arena->res = res_size;
    return arena;
}

void arena_release(Arena* arena) {
    ASSERT(arena != nullptr, "arena must not be null");
    for(Arena *block = arena->current, *prev = nullptr; block != nullptr;
        block = prev) {
        prev = block->prev;
        release_system_memory(block, block->res);
    }
}

void* arena_push(Arena* arena, u64 size, u64 align, bool zero) {
    ASSERT(arena != nullptr, "arena must not be null");
    Arena* current = arena->current;

    u64 pos_pre = align_up(current->pos, align);
    u64 pos_post = pos_pre + size;

    if(pos_post > current->res) {
        u64 new_res = current->res_size;
        u64 new_cmt = current->cmt_size;
        if(size + ARENA_HEADER_SIZE > new_res) {
            u64 page = get_system_page_size();
            new_res = align_up(size + ARENA_HEADER_SIZE, page);
            new_cmt = new_res;
        }
        Arena* block = arena_alloc(new_res, new_cmt);
        block->base_pos = current->base_pos + current->res;
        block->res_size = current->res_size;
        block->cmt_size = current->cmt_size;
        SLL_STACK_PUSH_N(arena->current, block, prev);
        current = block;
        pos_pre = align_up(current->pos, align);
        pos_post = pos_pre + size;
    }

    if(pos_post > current->cmt) {
        u64 new_cmt = align_up(pos_post, current->cmt_size);
        if(new_cmt > current->res) {
            new_cmt = current->res;
        }
        commit_system_memory(
            (u8*)current + current->cmt,
            new_cmt - current->cmt
        );
        current->cmt = new_cmt;
    }

    void* result = (u8*)current + pos_pre;
    current->pos = pos_post;
    if(zero) {
        memset(result, 0, size);
    }
    return result;
}

u64 arena_pos(Arena* arena) {
    ASSERT(arena != nullptr, "arena must not be null");
    return arena->current->base_pos + arena->current->pos;
}

void arena_pop_to(Arena* arena, u64 pos) {
    ASSERT(arena != nullptr, "arena must not be null");
    u64 big_pos = pos < ARENA_HEADER_SIZE ? (u64)ARENA_HEADER_SIZE : pos;
    Arena* current = arena->current;
    while(current->base_pos >= big_pos) {
        Arena* prev = current->prev;
        release_system_memory(current, current->res);
        current = prev;
    }
    arena->current = current;
    u64 new_pos = big_pos - current->base_pos;
    ASSERT(new_pos <= current->pos, "arena_pop_to: new_pos out of range");
    current->pos = new_pos;
}

void arena_clear(Arena* arena) {
    arena_pop_to(arena, 0);
}

Temp temp_begin(Arena* arena) {
    ASSERT(arena != nullptr, "arena must not be null");
    Temp result = {arena, arena_pos(arena)};
    return result;
}

void temp_end(Temp t) {
    arena_pop_to(t.arena, t.pos);
}
