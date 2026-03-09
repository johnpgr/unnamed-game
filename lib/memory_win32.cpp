#include "defines.h"
#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <windows.h>

static void os_memory_fail(const char* operation) {
    DWORD error = GetLastError();
    log_fatal("%s failed with error %lu", operation, (unsigned long)error);
    abort();
}

void* os_memory_reserve(u64 size) {
    // PAGE_NOACCESS ensures no RAM is used and any access crashes (good for debugging)
    void* ptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    if (ptr == NULL) {
        os_memory_fail("VirtualAlloc reserve");
    }
    return ptr;
}

u64 os_memory_page_size() {
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    return (u64)system_info.dwPageSize;
}

void os_memory_commit(void* ptr, u64 size) {
    if (size == 0) return;

    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if (result == NULL) {
        os_memory_fail("VirtualAlloc commit");
    }
}

void os_memory_decommit(void* ptr, u64 size) {
    if (size == 0) return;
    if (VirtualFree(ptr, size, MEM_DECOMMIT) == 0) {
        os_memory_fail("VirtualFree decommit");
    }
}

void os_memory_release(void* ptr, u64 size) {
    (void)size;
    if (ptr == NULL) return;

    // Size must be 0 when using MEM_RELEASE
    if (VirtualFree(ptr, 0, MEM_RELEASE) == 0) {
        os_memory_fail("VirtualFree release");
    }
}
