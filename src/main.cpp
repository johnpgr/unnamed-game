#include "base/arena.h"
#include "base/core.h"
#include "base/log.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "renderer/vulkan.cpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

struct AppState {
    Arena permanent_arena;
    Arena transient_arena;
    PushCmdBuffer render_cmds;
};

internal void build_frame(AppState *app_state) {
    assert(app_state != nullptr, "App state must not be null!");

    push_cmd_buffer_reset(&app_state->render_cmds);
    push_clear(&app_state->render_cmds, vec4(0.04f, 0.05f, 0.08f, 1.0f));
}

int main(void) {
    int result = 0;
    GLFWwindow *window = nullptr;
    bool glfw_initialized = false;
    bool renderer_initialized = false;
    AppState app_state = {};

    app_state.permanent_arena = create_arena(MB);
    app_state.transient_arena = create_arena(MB);
    app_state.render_cmds =
        create_push_cmd_buffer(&app_state.permanent_arena, (u32)(128 * KB));

    if(!glfwInit()) {
        LOG_FATAL("glfwInit failed.");
        result = -1;
        goto cleanup;
    }
    glfw_initialized = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        "editor",
        nullptr,
        nullptr
    );
    if(window == nullptr) {
        char const *description = nullptr;
        glfwGetError(&description);
        LOG_FATAL(
            "glfwCreateWindow failed: %s",
            description != nullptr ? description : "Unknown error"
        );
        result = -1;
        goto cleanup;
    }

    if(!init_vulkan(&app_state.permanent_arena, window)) {
        result = -1;
        goto cleanup;
    }
    renderer_initialized = true;

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            continue;
        }

        clear_arena(&app_state.transient_arena);
        build_frame(&app_state);

        if(!begin_frame()) {
            result = -1;
            break;
        }

        if(!render_drain_cmd_buffer(&app_state.render_cmds)) {
            result = -1;
            break;
        }
    }

cleanup:
    if(renderer_initialized) {
        cleanup_vulkan();
    }
    if(window != nullptr) {
        glfwDestroyWindow(window);
    }
    if(glfw_initialized) {
        glfwTerminate();
    }
    release_arena(&app_state.transient_arena);
    release_arena(&app_state.permanent_arena);

    return result;
}
