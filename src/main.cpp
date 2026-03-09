#include "defines.h"

#if OS_WINDOWS
#include "memory_win32.cpp"
#else
#include "memory_posix.cpp"
#endif

int main(int argc, char* argv[]) {
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
