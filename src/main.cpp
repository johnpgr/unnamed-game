#include "base/defines.h"

#if OS_LINUX
#include "memory/memory_linux.cpp"
#include "platform/platform_linux.cpp"
#elif OS_MAC
#include "memory/memory_macos.cpp"
#elif OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include "memory/memory_win32.cpp"
#include "platform/platform_win32.cpp"
#else
#error "Unsupported platform"
#endif

#include "renderer/vulkan.cpp"

typedef int (*GameTestSymbolFn)(void);

MAIN {
    (void)rvkDrawFrame;

    Arena global_arena = Arena::make();
    pwCreateWindow("Unnammed game", WIDTH, HEIGHT);
    pwSetWindowResizable(true);

    if (!rvkCreateRenderer(&global_arena)) {
        global_arena.release();
        return -1;
    };

    pwShowWindow();

    while (!pwShouldCloseWindow()) {
        pwPollEvents();
    }

    rvkDestroyRenderer();
    global_arena.release();
    return 0;
}
