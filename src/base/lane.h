#pragma once

#include "base/core.h"

#if !OS_WINDOWS
#include <sched.h>
#endif

#define MAX_LANES 64

struct LaneBarrier {
    volatile s64 count;
    volatile s64 generation;
    s64 thread_count;
    volatile u64 shared_u64;
};

struct LaneContext {
    u32 lane_idx;
    u32 lane_count;
    LaneBarrier *barrier;
};

global_variable thread_local LaneContext *tl_lane_context = nullptr;

internal void
init_lane_barrier(LaneBarrier *barrier, u32 thread_count) {
    assert(barrier != nullptr, "Lane barrier must not be null!");
    barrier->count = 0;
    barrier->generation = 0;
    barrier->thread_count = thread_count;
    barrier->shared_u64 = 0;
}

internal void
set_lane_context(LaneContext *context) {
    tl_lane_context = context;
}

internal LaneContext *
get_lane_context(void) {
    assert(tl_lane_context != nullptr, "Lane context must be installed!");
    return tl_lane_context;
}

internal u32
lane_idx(void) {
    return get_lane_context()->lane_idx;
}

internal u32
lane_count(void) {
    return get_lane_context()->lane_count;
}

internal void
lane_pause(void) {
#if (COMPILER_CLANG || COMPILER_GCC) && defined(__i386__)
    __builtin_ia32_pause();
#elif (COMPILER_CLANG || COMPILER_GCC) && defined(__x86_64__)
    __builtin_ia32_pause();
#elif (COMPILER_CLANG || COMPILER_GCC) && defined(__aarch64__)
    __asm__ __volatile__("yield");
#elif !OS_WINDOWS
    sched_yield();
#endif
}

internal void
lane_sync(void) {
    LaneBarrier *barrier = get_lane_context()->barrier;
    if(barrier->thread_count <= 1) {
        return;
    }

    s64 generation = barrier->generation;
    s64 arrived = __sync_add_and_fetch(&barrier->count, 1);
    if(arrived == barrier->thread_count) {
        __sync_synchronize();
        __sync_lock_test_and_set(&barrier->count, 0);
        __sync_add_and_fetch(&barrier->generation, 1);
    } else {
        while(barrier->generation == generation) {
            lane_pause();
        }
        __sync_synchronize();
    }
}

struct LaneRange {
    u64 min;
    u64 max;
};

internal LaneRange
lane_range(u64 count) {
    u64 idx = lane_idx();
    u64 total = lane_count();
    u64 per_lane = count / total;
    u64 leftover = count % total;
    b32 gets_leftover = (idx < leftover);
    u64 leftovers_before = gets_leftover ? idx : leftover;
    u64 first = (per_lane * idx) + leftovers_before;
    u64 opl = first + per_lane + (gets_leftover ? 1 : 0);

    LaneRange result = {first, opl};
    return result;
}

internal void
lane_sync_u64(u64 *value, u32 source_lane) {
    assert(value != nullptr, "Broadcast value must not be null!");

    LaneBarrier *barrier = get_lane_context()->barrier;
    if(barrier->thread_count <= 1) {
        return;
    }

    if(lane_idx() == source_lane) {
        barrier->shared_u64 = *value;
    }
    lane_sync();

    if(lane_idx() != source_lane) {
        *value = barrier->shared_u64;
    }
    lane_sync();
}
