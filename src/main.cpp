#include "base/arena.h"
#include "base/core.h"
#include "base/log.h"

#include "game_api.h"

#include <pthread.h>

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

struct GameCode {
    void *library;
    GameUpdateAndRender *update_and_render;
    b32 is_valid;
};

struct FrameDispatch {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    u64 frame_index;
    b32 shutdown;
};

struct WorkerThreadParams {
    LaneContext lane_context;
    GameFrameContext game_frame_context;
    GameCode *game_code;
    GameMemory *game_memory;
    GameInput *input;
    FrameDispatch *dispatch;
};

global_variable Arena global_arena;
global_variable GLFWwindow *global_window;
global_variable b32 global_glfw_initialized;
global_variable b32 global_renderer_initialized;
global_variable GameCode global_game_code;
global_variable GameMemory global_game_memory;

internal void
unload_game_code(GameCode *game_code) {
    if(game_code->library != nullptr) {
        dlclose(game_code->library);
    }

    game_code->library = nullptr;
    game_code->update_and_render = nullptr;
    game_code->is_valid = false;
}

internal b32
get_executable_directory(char *buffer, u64 buffer_size) {
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

internal b32
build_game_library_path(char *buffer, u64 buffer_size) {
    assert(buffer != nullptr, "Game path buffer must not be null!");

    char executable_directory[4096] = {};
    if(!get_executable_directory(
           executable_directory,
           sizeof(executable_directory)
       )) {
        return false;
    }

#if OS_MAC
    char const *library_name = "libgame.dylib";
#elif OS_LINUX
    char const *library_name = "libgame.so";
#else
    char const *library_name = "libgame";
#endif

    int written = snprintf(
        buffer,
        buffer_size,
        "%s/%s",
        executable_directory,
        library_name
    );
    return (written > 0) && ((u64)written < buffer_size);
}

internal b32
load_game_code(GameCode *game_code) {
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

internal void
cleanup(void) {
    unload_game_code(&global_game_code);

    if(global_renderer_initialized) {
        cleanup_vulkan();
        global_renderer_initialized = false;
    }

    if(global_window != nullptr) {
        glfwDestroyWindow(global_window);
        global_window = nullptr;
    }

    if(global_glfw_initialized) {
        glfwTerminate();
        global_glfw_initialized = false;
    }

    release_arena(&global_arena);
}

internal void
update_input(GLFWwindow *window, GameInput *input) {
    assert(window != nullptr, "Window must not be null!");
    assert(input != nullptr, "Input must not be null!");

    input->move_up.ended_down = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    input->move_down.ended_down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    input->move_left.ended_down = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    input->move_right.ended_down = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
}

internal u32
get_lane_count_from_system(void) {
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if(cpu_count < 1) {
        cpu_count = 1;
    }
    if(cpu_count > MAX_LANES) {
        cpu_count = MAX_LANES;
    }

    return (u32)cpu_count;
}

internal b32
init_frame_dispatch(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");

    *dispatch = {};
    if(pthread_mutex_init(&dispatch->mutex, nullptr) != 0) {
        return false;
    }
    if(pthread_cond_init(&dispatch->cond, nullptr) != 0) {
        pthread_mutex_destroy(&dispatch->mutex);
        return false;
    }

    return true;
}

internal void
destroy_frame_dispatch(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");
    pthread_cond_destroy(&dispatch->cond);
    pthread_mutex_destroy(&dispatch->mutex);
}

internal void
signal_frame_start(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");

    pthread_mutex_lock(&dispatch->mutex);
    ++dispatch->frame_index;
    pthread_cond_broadcast(&dispatch->cond);
    pthread_mutex_unlock(&dispatch->mutex);
}

internal void
signal_shutdown(FrameDispatch *dispatch) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");

    pthread_mutex_lock(&dispatch->mutex);
    dispatch->shutdown = true;
    pthread_cond_broadcast(&dispatch->cond);
    pthread_mutex_unlock(&dispatch->mutex);
}

internal b32
wait_for_frame_start(FrameDispatch *dispatch, u64 *frame_index) {
    assert(dispatch != nullptr, "Frame dispatch must not be null!");
    assert(frame_index != nullptr, "Frame index must not be null!");

    pthread_mutex_lock(&dispatch->mutex);
    while(!dispatch->shutdown && *frame_index == dispatch->frame_index) {
        pthread_cond_wait(&dispatch->cond, &dispatch->mutex);
    }

    b32 should_run = !dispatch->shutdown;
    *frame_index = dispatch->frame_index;
    pthread_mutex_unlock(&dispatch->mutex);
    return should_run;
}

internal void
init_render_commands(Arena *arena, RenderCommands *commands, u32 lane_count) {
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

internal void *
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

    return nullptr;
}

int
main(void) {
    int result = 0;
    char const *description = nullptr;
    f64 last_counter = 0.0;
    u32 lane_count = get_lane_count_from_system();
    LaneBarrier frame_barrier = {};
    LaneContext main_lane_context = {};
    GameFrameContext main_game_frame_context = {};
    FrameDispatch frame_dispatch = {};
    b32 frame_dispatch_initialized = false;
    pthread_t worker_threads[MAX_LANES] = {};
    WorkerThreadParams worker_params[MAX_LANES] = {};
    u32 worker_thread_count = 0;
    GameInput input = {};

    global_arena = create_arena();
    global_game_memory.permanent_storage_size = PERMANENT_STORAGE_SIZE;
    global_game_memory.permanent_storage =
        push_size(&global_arena, PERMANENT_STORAGE_SIZE);
    global_game_memory.transient_storage_size = TRANSIENT_STORAGE_SIZE;
    global_game_memory.transient_storage =
        push_size(&global_arena, TRANSIENT_STORAGE_SIZE);
    global_game_memory.render_commands =
        push_struct(&global_arena, RenderCommands);
    init_render_commands(
        &global_arena,
        global_game_memory.render_commands,
        lane_count
    );

    init_lane_barrier(&frame_barrier, lane_count);
    main_lane_context.lane_idx = 0;
    main_lane_context.lane_count = lane_count;
    main_lane_context.barrier = &frame_barrier;
    main_game_frame_context.lane = &main_lane_context;
    set_lane_context(&main_lane_context);

    if(!init_frame_dispatch(&frame_dispatch)) {
        LOG_FATAL("Failed to initialize frame dispatch.");
        result = -1;
        goto cleanup_label;
    }
    frame_dispatch_initialized = true;

    if(!glfwInit()) {
        LOG_FATAL("glfwInit failed");
        result = -1;
        goto cleanup_label;
    }
    global_glfw_initialized = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    global_window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        "The Game",
        nullptr,
        nullptr
    );
    if(global_window == nullptr) {
        glfwGetError(&description);
        LOG_FATAL(
            "glfwCreateWindow failed: %s",
            description != nullptr ? description : "Unknown error"
        );
        result = -1;
        goto cleanup_label;
    }

    if(!load_game_code(&global_game_code)) {
        result = -1;
        goto cleanup_label;
    }

    if(!init_vulkan(&global_arena, global_window, lane_count)) {
        result = -1;
        goto cleanup_label;
    }
    global_renderer_initialized = true;

    for(u32 lane_index = 1; lane_index < lane_count; ++lane_index) {
        WorkerThreadParams *params = worker_params + lane_index;
        params->lane_context.lane_idx = lane_index;
        params->lane_context.lane_count = lane_count;
        params->lane_context.barrier = &frame_barrier;
        params->game_frame_context.lane = &params->lane_context;
        params->game_code = &global_game_code;
        params->game_memory = &global_game_memory;
        params->input = &input;
        params->dispatch = &frame_dispatch;

        if(pthread_create(
               worker_threads + lane_index,
               nullptr,
               game_worker_thread,
               params
           ) != 0) {
            LOG_FATAL("Failed to create worker thread %u.", lane_index);
            result = -1;
            goto cleanup_label;
        }

        ++worker_thread_count;
    }

    last_counter = glfwGetTime();
    while(!glfwWindowShouldClose(global_window)) {
        glfwPollEvents();

        f64 current_counter = glfwGetTime();
        input = {};
        input.dt_for_frame = (f32)(current_counter - last_counter);
        last_counter = current_counter;
        update_input(global_window, &input);

        if(!begin_frame()) {
            result = -1;
            break;
        }

        RenderCommands *commands = global_game_memory.render_commands;
        reset_render_commands(commands);
        commands->screen_width = vk_state.swapchain_extent.width;
        commands->screen_height = vk_state.swapchain_extent.height;

        signal_frame_start(&frame_dispatch);

        global_game_code.update_and_render(
            &global_game_memory,
            &input,
            &main_game_frame_context
        );
        if(!render_group_to_output(commands)) {
            result = -1;
            break;
        }
    }

cleanup_label:
    if(frame_dispatch_initialized) {
        signal_shutdown(&frame_dispatch);
    }
    for(u32 lane_index = 1; lane_index <= worker_thread_count; ++lane_index) {
        pthread_join(worker_threads[lane_index], nullptr);
    }
    if(frame_dispatch_initialized) {
        destroy_frame_dispatch(&frame_dispatch);
    }
    cleanup();
    return result;
}
