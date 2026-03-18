#include "base/arena.h"
#include "base/core.h"
#include "base/log.h"

#include <SDL3/SDL.h>

#include "renderer/vulkan.cpp"

#define WIDTH 800
#define HEIGHT 600

static Arena global_arena = {};
static SDL_Window* window = nullptr;
static bool sdl_initialized = false;
static bool renderer_initialized = false;

static void Cleanup(void) {
    if (renderer_initialized) {
        CleanupVulkan();
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    if (sdl_initialized) {
        SDL_Quit();
        sdl_initialized = false;
    }
    global_arena.release();
}

int main() {
    global_arena = CreateArena();

    defer {
        Cleanup();
    };

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        LOG_FATAL("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }
    sdl_initialized = true;

    window = SDL_CreateWindow("The Game", WIDTH, HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        LOG_FATAL("SDL_CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    if (!InitVulkan(&global_arena, window)) {
        return -1;
    }
    renderer_initialized = true;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        if (!Draw()) {
            running = false;
        }
    }

    return 0;
}
