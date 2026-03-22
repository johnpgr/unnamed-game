#include "base/arena.h"
#include "base/core.h"
#include "base/log.h"
#include "base/threads/threads_win32.cpp"
#include "base/threads/threads_posix.cpp"

#include "game_api.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#if OS_MAC
#include <mach-o/dyld.h>
#endif

#include "renderer/vulkan.cpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define PERMANENT_STORAGE_SIZE (64 * MB)
#define TRANSIENT_STORAGE_SIZE (64 * MB)

#if OS_WINDOWS
#define DYNLIB(name) name ".dll"
#elif OS_MAC
#define DYNLIB(name) "lib" name ".dylib"
#elif OS_LINUX
#define DYNLIB(name) "lib" name ".so"
#else
#define DYNLIB(name) name
#endif

struct GameCode {
    void *library;
    GameUpdateAndRender *update_and_render;
    bool is_valid;
};

struct FrameLoopState {
    GameCode *game_code;
    GameMemory *game_memory;
    GameInput *input;
    GLFWwindow *window;
    volatile bool startup_complete;
    volatile bool running;
    volatile bool frame_failed;
    f64 last_counter;
};

struct WorkerThreadParams {
    LaneContext lane_context;
    FrameLoopState *frame_loop_state;
};

internal void unload_game_code(GameCode *game_code) {
    if(game_code->library != nullptr) {
        dlclose(game_code->library);
    }

    game_code->library = nullptr;
    game_code->update_and_render = nullptr;
    game_code->is_valid = false;
}

internal bool get_executable_directory(char *buffer, u64 buffer_size) {
    assert(buffer != nullptr, "Executable path buffer must not be null!");
    assert(buffer_size > 0, "Executable path buffer must not be empty!");

#if OS_MAC
    u32 path_size = (u32)buffer_size;
    if(_NSGetExecutablePath(buffer, &path_size) != 0) {
        return false;
    }

    buffer[buffer_size - 1] = 0;
#elif OS_LINUX
    ssize_t size_read = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if(size_read <= 0) {
        return false;
    }

    buffer[size_read] = 0;
#else
    return false;
#endif

    char *last_slash = strrchr(buffer, '/');
    if(last_slash == nullptr) {
        return false;
    }

    *last_slash = 0;
    return true;
}

internal bool build_game_library_path(char *buffer, u64 buffer_size) {
    assert(buffer != nullptr, "Game path buffer must not be null!");

    char executable_directory[4096] = {};
    if(!get_executable_directory(
           executable_directory,
           sizeof(executable_directory)
       )) {
        return false;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "%s/%s",
        executable_directory,
        DYNLIB("game")
    );
    return (written > 0) && ((u64)written < buffer_size);
}

internal bool load_game_code(GameCode *game_code) {
    assert(game_code != nullptr, "Game code must not be null!");

    char library_path[4096] = {};
    if(!build_game_library_path(library_path, sizeof(library_path))) {
        LOG_FATAL("Failed to build game library path.");
        return false;
    }

    void *library = dlopen(library_path, RTLD_NOW);
    if(library == nullptr) {
        char const *error = dlerror();
        LOG_FATAL(
            "dlopen failed for %s: %s",
            library_path,
            error != nullptr ? error : "Unknown error"
        );
        return false;
    }

    GameUpdateAndRender *update_and_render =
        (GameUpdateAndRender *)dlsym(library, "game_update_and_render");
    if(update_and_render == nullptr) {
        char const *error = dlerror();
        LOG_FATAL(
            "dlsym failed for game_update_and_render: %s",
            error != nullptr ? error : "Unknown error"
        );
        dlclose(library);
        return false;
    }

    game_code->library = library;
    game_code->update_and_render = update_and_render;
    game_code->is_valid = true;
    return true;
}

internal void update_input(GLFWwindow *window, GameInput *input) {
    assert(window != nullptr, "Window must not be null!");
    assert(input != nullptr, "Input must not be null!");

    input->move_up.ended_down = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    input->move_down.ended_down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    input->move_left.ended_down = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    input->move_right.ended_down = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
}

internal void init_render_commands(
    Arena *arena,
    RenderCommands *commands,
    u32 lane_count
) {
    assert(arena != nullptr, "Arena must not be null!");
    assert(commands != nullptr, "Render commands must not be null!");
    assert(lane_count > 0, "Lane count must be non-zero!");

    commands->active_lane_count = lane_count;
    commands->screen_width = WINDOW_WIDTH;
    commands->screen_height = WINDOW_HEIGHT;

    for(u32 lane_index = 0; lane_index < lane_count; ++lane_index) {
        LanePushBuffer *buffer = commands->lane_buffers + lane_index;
        buffer->base = (u8 *)push_size(arena, PUSH_BUFFER_SIZE_PER_LANE);
        buffer->capacity = PUSH_BUFFER_SIZE_PER_LANE;
        buffer->used = 0;
        buffer->entry_count = 0;
    }
}

internal void run_frame_loop(WorkerThreadParams *params) {
    assert(params != nullptr, "Worker params must not be null!");
    assert(
        params->frame_loop_state != nullptr,
        "Frame loop state must not be null!"
    );

    FrameLoopState *frame_loop_state = params->frame_loop_state;
    GameFrameContext game_frame_context = {};
    game_frame_context.lane = &params->lane_context;

    set_lane_context(&params->lane_context);

    while(!frame_loop_state->startup_complete) {
        lane_pause();
    }

    while(frame_loop_state->running) {
        if(lane_idx() == 0) {
            glfwPollEvents();
            frame_loop_state->running =
                !glfwWindowShouldClose(frame_loop_state->window);

            if(frame_loop_state->running) {
                f64 current_counter = glfwGetTime();
                *frame_loop_state->input = {};
                frame_loop_state->input->dt_for_frame =
                    (f32)(current_counter - frame_loop_state->last_counter);
                frame_loop_state->last_counter = current_counter;
                update_input(frame_loop_state->window, frame_loop_state->input);

                if(!begin_frame()) {
                    frame_loop_state->frame_failed = true;
                    frame_loop_state->running = false;
                } else {
                    RenderCommands *commands =
                        frame_loop_state->game_memory->render_commands;
                    commands->screen_width = vk_state.swapchain_extent.width;
                    commands->screen_height = vk_state.swapchain_extent.height;
                }
            }
        }

        lane_sync();
        if(!frame_loop_state->running) {
            break;
        }

        RenderCommands *commands =
            frame_loop_state->game_memory->render_commands;
        LanePushBuffer *lane_buffer = commands->lane_buffers + lane_idx();
        lane_buffer->used = 0;
        lane_buffer->entry_count = 0;

        frame_loop_state->game_code->update_and_render(
            frame_loop_state->game_memory,
            frame_loop_state->input,
            &game_frame_context
        );

        if(!render_group_to_output(commands) && lane_idx() == 0) {
            frame_loop_state->frame_failed = true;
            frame_loop_state->running = false;
        }

        lane_sync();
    }
}

internal ThreadProcResult THREAD_PROC_CALL
game_worker_thread(void *raw_params) {
    WorkerThreadParams *params = (WorkerThreadParams *)raw_params;
    run_frame_loop(params);

    return THREAD_PROC_SUCCESS;
}

int main(void) {
    int result = 0;
    char const *description = nullptr;
    u32 lane_count = clamp(get_logical_processor_count(), 1U, MAX_LANES);
    LaneBarrier frame_barrier = {};
    Thread worker_threads[MAX_LANES] = {};
    WorkerThreadParams worker_params[MAX_LANES] = {};
    u32 worker_thread_count = 0;
    GameInput input = {};
    Arena arena = {};
    GLFWwindow *window = nullptr;
    bool glfw_initialized = false;
    bool renderer_initialized = false;
    GameCode game_code = {};
    GameMemory game_memory = {};
    FrameLoopState frame_loop_state = {};
    WorkerThreadParams *main_params = worker_params;

    arena = create_arena();
    game_memory.permanent_storage_size = PERMANENT_STORAGE_SIZE;
    game_memory.permanent_storage = push_size(&arena, PERMANENT_STORAGE_SIZE);
    game_memory.transient_storage_size = TRANSIENT_STORAGE_SIZE;
    game_memory.transient_storage = push_size(&arena, TRANSIENT_STORAGE_SIZE);
    game_memory.render_commands = push_struct(&arena, RenderCommands);
    init_render_commands(&arena, game_memory.render_commands, lane_count);

    frame_loop_state.game_code = &game_code;
    frame_loop_state.game_memory = &game_memory;
    frame_loop_state.input = &input;
    frame_loop_state.startup_complete = false;
    frame_loop_state.running = true;
    frame_loop_state.frame_failed = false;

    if(!glfwInit()) {
        LOG_FATAL("glfwInit failed");
        result = -1;
        goto cleanup;
    }
    glfw_initialized = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        "The Game",
        nullptr,
        nullptr
    );
    if(window == nullptr) {
        glfwGetError(&description);
        LOG_FATAL(
            "glfwCreateWindow failed: %s",
            description != nullptr ? description : "Unknown error"
        );
        result = -1;
        goto cleanup;
    }

    if(!load_game_code(&game_code)) {
        result = -1;
        goto cleanup;
    }

    if(!init_vulkan(&arena, window, lane_count)) {
        result = -1;
        goto cleanup;
    }
    renderer_initialized = true;

    main_params->lane_context.lane_idx = 0;
    main_params->lane_context.barrier = &frame_barrier;
    main_params->frame_loop_state = &frame_loop_state;

    for(u32 lane_index = 1; lane_index < lane_count; ++lane_index) {
        WorkerThreadParams *params = worker_params + lane_index;
        params->lane_context.lane_idx = lane_index;
        params->lane_context.barrier = &frame_barrier;
        params->frame_loop_state = &frame_loop_state;

        if(!create_thread(
               worker_threads + lane_index,
               game_worker_thread,
               params
           )) {
            LOG_WARN(
                "Failed to create worker thread %u. Falling back to %u lanes.",
                lane_index,
                lane_index
            );
            lane_count = lane_index;
            break;
        }

        ++worker_thread_count;
    }

    for(u32 lane_index = 0; lane_index < lane_count; ++lane_index) {
        worker_params[lane_index].lane_context.lane_count = lane_count;
    }

    game_memory.render_commands->active_lane_count = lane_count;
    vk_state.active_lane_count = lane_count;
    init_lane_barrier(&frame_barrier, lane_count);

    frame_loop_state.window = window;
    frame_loop_state.last_counter = glfwGetTime();
    frame_loop_state.startup_complete = true;

    run_frame_loop(main_params);
    if(frame_loop_state.frame_failed) {
        result = -1;
    }

cleanup:
    frame_loop_state.running = false;
    frame_loop_state.startup_complete = true;
    for(u32 lane_index = 1; lane_index <= worker_thread_count; ++lane_index)
        join_thread(worker_threads + lane_index);
    unload_game_code(&game_code);
    if(renderer_initialized)
        cleanup_vulkan();
    if(window != nullptr)
        glfwDestroyWindow(window);
    if(glfw_initialized)
        glfwTerminate();
    release_arena(&arena);

    return result;
}
