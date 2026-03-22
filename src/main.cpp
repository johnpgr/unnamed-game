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

struct FrameDispatch {
    ThreadMutex mutex;
    ThreadConditionVariable cond;
    u64 frame_index;
    bool shutdown;
};

struct WorkerThreadParams {
    LaneContext lane_context;
    GameFrameContext game_frame_context;
    GameCode *game_code;
    GameMemory *game_memory;
    GameInput *input;
    FrameDispatch *dispatch;
};

internal void destroy_frame_dispatch(FrameDispatch *dispatch);
internal void signal_shutdown(FrameDispatch *dispatch);

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

internal bool init_frame_dispatch(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");

    *dispatch = {};
    if(!init_thread_mutex(&dispatch->mutex)) {
        return false;
    }
    if(!init_thread_condition_variable(&dispatch->cond)) {
        destroy_thread_mutex(&dispatch->mutex);
        return false;
    }

    return true;
}

internal void destroy_frame_dispatch(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");
    destroy_thread_condition_variable(&dispatch->cond);
    destroy_thread_mutex(&dispatch->mutex);
}

internal void signal_frame_start(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");

    lock_thread_mutex(&dispatch->mutex);
    ++dispatch->frame_index;
    wake_all_thread_condition_variable(&dispatch->cond);
    unlock_thread_mutex(&dispatch->mutex);
}

internal void signal_shutdown(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");

    lock_thread_mutex(&dispatch->mutex);
    dispatch->shutdown = true;
    wake_all_thread_condition_variable(&dispatch->cond);
    unlock_thread_mutex(&dispatch->mutex);
}

internal bool wait_for_frame_start(FrameDispatch *dispatch, u64 *frame_index) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");
    assert(frame_index != nullptr, "Frame index must not be null!");

    lock_thread_mutex(&dispatch->mutex);
    while(!dispatch->shutdown && *frame_index == dispatch->frame_index) {
        wait_thread_condition_variable(&dispatch->cond, &dispatch->mutex);
    }

    bool should_run = !dispatch->shutdown;
    *frame_index = dispatch->frame_index;
    unlock_thread_mutex(&dispatch->mutex);
    return should_run;
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

internal ThreadProcResult THREAD_PROC_CALL
game_worker_thread(void *raw_params) {
    WorkerThreadParams *params = (WorkerThreadParams *)raw_params;
    set_lane_context(&params->lane_context);

    u64 frame_index = 0;
    while(wait_for_frame_start(params->dispatch, &frame_index)) {
        params->game_code->update_and_render(
            params->game_memory,
            params->input,
            &params->game_frame_context
        );
        render_group_to_output(params->game_memory->render_commands);
    }

    return THREAD_PROC_SUCCESS;
}

int main(void) {
    int result = 0;
    char const *description = nullptr;
    f64 last_counter = 0.0;
    u32 lane_count = clamp(get_logical_processor_count(), 1U, MAX_LANES);
    LaneBarrier frame_barrier = {};
    LaneContext main_lane_context = {};
    GameFrameContext main_game_frame_context = {};
    FrameDispatch frame_dispatch = {};
    bool frame_dispatch_initialized = false;
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

    arena = create_arena();
    game_memory.permanent_storage_size = PERMANENT_STORAGE_SIZE;
    game_memory.permanent_storage = push_size(&arena, PERMANENT_STORAGE_SIZE);
    game_memory.transient_storage_size = TRANSIENT_STORAGE_SIZE;
    game_memory.transient_storage = push_size(&arena, TRANSIENT_STORAGE_SIZE);
    game_memory.render_commands = push_struct(&arena, RenderCommands);
    init_render_commands(&arena, game_memory.render_commands, lane_count);

    init_lane_barrier(&frame_barrier, lane_count);
    main_lane_context.lane_idx = 0;
    main_lane_context.lane_count = lane_count;
    main_lane_context.barrier = &frame_barrier;
    main_game_frame_context.lane = &main_lane_context;
    set_lane_context(&main_lane_context);

    if(!init_frame_dispatch(&frame_dispatch)) {
        LOG_FATAL("Failed to initialize frame dispatch.");
        result = -1;
        goto cleanup;
    }
    frame_dispatch_initialized = true;

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

    for(u32 lane_index = 1; lane_index < lane_count; ++lane_index) {
        WorkerThreadParams *params = worker_params + lane_index;
        params->lane_context.lane_idx = lane_index;
        params->lane_context.lane_count = lane_count;
        params->lane_context.barrier = &frame_barrier;
        params->game_frame_context.lane = &params->lane_context;
        params->game_code = &game_code;
        params->game_memory = &game_memory;
        params->input = &input;
        params->dispatch = &frame_dispatch;

        if(!create_thread(
               worker_threads + lane_index,
               game_worker_thread,
               params
           )) {
            LOG_FATAL("Failed to create worker thread %u.", lane_index);
            result = -1;
            goto cleanup;
        }

        ++worker_thread_count;
    }

    last_counter = glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        f64 current_counter = glfwGetTime();
        input = {};
        input.dt_for_frame = (f32)(current_counter - last_counter);
        last_counter = current_counter;
        update_input(window, &input);

        if(!begin_frame()) {
            result = -1;
            break;
        }

        RenderCommands *commands = game_memory.render_commands;
        reset_render_commands(commands);
        commands->screen_width = vk_state.swapchain_extent.width;
        commands->screen_height = vk_state.swapchain_extent.height;

        signal_frame_start(&frame_dispatch);

        game_code
            .update_and_render(&game_memory, &input, &main_game_frame_context);
        if(!render_group_to_output(commands)) {
            result = -1;
            break;
        }
    }

cleanup:
    if(frame_dispatch_initialized)
        signal_shutdown(&frame_dispatch);
    for(u32 lane_index = 1; lane_index <= worker_thread_count; ++lane_index)
        join_thread(worker_threads + lane_index);
    if(frame_dispatch_initialized)
        destroy_frame_dispatch(&frame_dispatch);
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
