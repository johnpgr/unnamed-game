#include "base/defines.h"

#if OS_WINDOWS
#include "platform/platform_win32.cpp"
#elif OS_LINUX
#include "platform/platform_linux.cpp"
#elif OS_MAC
#include "platform/platform_macos.cpp"
#else
#endif

#if OS_WINDOWS
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd
) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;
#else
int main(int argc, char* argv[]) {
#endif
    (void)argc;
    (void)argv;

    Arena global_arena = Arena::make();

    ArrayList<i32> list = ArrayList<i32>::make(&global_arena);
    list.push(42);
    list.push(7);
    Array<i32> arr = list.to_array();
    i32 sum = 0;
    for (i32 value : arr) {
        sum += value;
    }

    String greeting = String::lit("Hello, world!");

    log_info(
        "%s (%d items, sum=%d)",
        greeting.to_cstr(&global_arena),
        (int)arr.count,
        sum
    );

    global_arena.release();
    return 0;
}
