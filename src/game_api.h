#pragma once

#include "base/core.h"
#include "base/lane.h"
#include "base/typedef.h"
#include "game/game_render_group.h"

struct GameButtonState {
    b32 ended_down;
};

struct GameInput {
    f32 dt_for_frame;
    GameButtonState move_up;
    GameButtonState move_down;
    GameButtonState move_left;
    GameButtonState move_right;
};

struct GameMemory {
    b32 is_initialized;
    u64 permanent_storage_size;
    void *permanent_storage;
    u64 transient_storage_size;
    void *transient_storage;
    RenderCommands *render_commands;
};

struct GameFrameContext {
    LaneContext *lane;
};

typedef void GameUpdateAndRender(
    GameMemory *memory,
    GameInput *input,
    GameFrameContext *frame_context
);

#define GAME_UPDATE_AND_RENDER(name)                                           \
    export void name(                                                          \
        GameMemory *memory,                                                    \
        GameInput *input,                                                      \
        GameFrameContext *frame_context                                        \
    )
