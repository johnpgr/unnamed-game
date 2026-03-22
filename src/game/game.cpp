#include "base/core.h"

#include "game/game_math.h"
#include "game/game_render_group.h"
#include "game_api.h"

struct Entity {
    vec2 p;
    vec2 dp;
    vec4 color;
    f32 width;
    f32 height;
};

struct GameState {
    u32 entity_count;
    Entity entities[4096];
};

internal void
clamp_entity_to_screen(Entity *entity, f32 screen_width, f32 screen_height) {
    assert(entity != nullptr, "Entity must not be null!");

    f32 half_width = 0.5f * entity->width;
    f32 half_height = 0.5f * entity->height;

    if(entity->p.x < half_width) {
        entity->p.x = half_width;
        entity->dp.x = -entity->dp.x;
    }
    if(entity->p.x > (screen_width - half_width)) {
        entity->p.x = screen_width - half_width;
        entity->dp.x = -entity->dp.x;
    }
    if(entity->p.y < half_height) {
        entity->p.y = half_height;
        entity->dp.y = -entity->dp.y;
    }
    if(entity->p.y > (screen_height - half_height)) {
        entity->p.y = screen_height - half_height;
        entity->dp.y = -entity->dp.y;
    }
}

internal void
init_game_state(GameState *state, u32 screen_width, u32 screen_height) {
    assert(state != nullptr, "Game state must not be null!");

    u32 width = (screen_width > 0) ? screen_width : 800;
    u32 height = (screen_height > 0) ? screen_height : 600;
    state->entity_count = ARRAY_COUNT(state->entities);

    u32 column_count = 64;
    f32 spacing_x = (f32)width / (f32)column_count;
    f32 spacing_y = 18.0f;

    for(u32 entity_index = 0; entity_index < state->entity_count;
        ++entity_index) {
        Entity *entity = state->entities + entity_index;
        u32 x_index = entity_index % column_count;
        u32 y_index = entity_index / column_count;

        entity->width = 12.0f;
        entity->height = 12.0f;
        entity->p = vec2(
            24.0f + ((f32)x_index * spacing_x),
            24.0f + ((f32)y_index * spacing_y)
        );
        entity->dp = vec2(
            ((entity_index & 1) != 0 ? 1.0f : -1.0f) *
                (20.0f + (f32)(entity_index % 11)),
            ((entity_index & 2) != 0 ? 1.0f : -1.0f) *
                (15.0f + (f32)(entity_index % 7))
        );
        entity->color = vec4(
            0.2f + 0.6f * ((f32)(x_index % 8) / 7.0f),
            0.2f + 0.6f * ((f32)(y_index % 8) / 7.0f),
            0.4f + 0.4f * ((f32)(entity_index % 5) / 4.0f),
            1.0f
        );
    }

    Entity *player = state->entities;
    player->p = vec2(0.5f * (f32)width, 0.5f * (f32)height);
    player->dp = vec2(0.0f, 0.0f);
    player->width = 28.0f;
    player->height = 28.0f;
    player->color = vec4(0.15f, 0.7f, 1.0f, 1.0f);
}

GAME_UPDATE_AND_RENDER(game_update_and_render) {
    assert(memory != nullptr, "Game memory must not be null!");
    assert(input != nullptr, "Game input must not be null!");
    assert(frame_context != nullptr, "Game frame context must not be null!");
    assert(
        frame_context->lane != nullptr,
        "Game lane context must not be null!"
    );
    assert(
        memory->permanent_storage != nullptr,
        "Permanent storage must not be null!"
    );
    assert(
        memory->render_commands != nullptr,
        "Render commands must not be null!"
    );
    assert(
        memory->permanent_storage_size >= sizeof(GameState),
        "Permanent storage too small!"
    );

    set_lane_context(frame_context->lane);

    GameState *state = (GameState *)memory->permanent_storage;
    RenderCommands *commands = memory->render_commands;

    if(!memory->is_initialized) {
        if(lane_idx() == 0) {
            init_game_state(
                state,
                commands->screen_width,
                commands->screen_height
            );
            memory->is_initialized = true;
        }
        lane_sync();
    }

    f32 screen_width =
        (commands->screen_width > 0) ? (f32)commands->screen_width : 800.0f;
    f32 screen_height =
        (commands->screen_height > 0) ? (f32)commands->screen_height : 600.0f;

    LaneRange range = lane_range(state->entity_count);
    for(u64 entity_index = range.min; entity_index < range.max;
        ++entity_index) {
        Entity *entity = state->entities + entity_index;
        if(entity_index == 0) {
            vec2 direction = {};
            if(input->move_left.ended_down) {
                direction.x -= 1.0f;
            }
            if(input->move_right.ended_down) {
                direction.x += 1.0f;
            }
            if(input->move_up.ended_down) {
                direction.y -= 1.0f;
            }
            if(input->move_down.ended_down) {
                direction.y += 1.0f;
            }

            entity->p += direction * (280.0f * input->dt_for_frame);
            clamp_entity_to_screen(entity, screen_width, screen_height);
        } else {
            entity->p += entity->dp * input->dt_for_frame;
            clamp_entity_to_screen(entity, screen_width, screen_height);
        }
    }

    lane_sync();

    if(lane_idx() == 0) {
        push_clear(commands, vec4(0.04f, 0.05f, 0.08f, 1.0f));
    }

    for(u64 entity_index = range.min; entity_index < range.max;
        ++entity_index) {
        Entity *entity = state->entities + entity_index;
        push_rect(
            commands,
            entity->p,
            entity->width,
            entity->height,
            entity->color,
            0.0f
        );
    }

    lane_sync();
}
