#if OS_WINDOWS

#include <windows.h>

internal void platform_memory_fail(const char* operation) {
    DWORD error = GetLastError();
    log_fatal("%s failed with error %lu", operation, (unsigned long)error);
    abort();
}

void* platform_memory_reserve(u64 size) {
    void* ptr = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (ptr == nullptr) {
        platform_memory_fail("VirtualAlloc reserve");
    }
    return ptr;
}

u64 platform_memory_page_size(void) {
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    return (u64)system_info.dwPageSize;
}

void platform_memory_commit(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if (result == nullptr) {
        platform_memory_fail("VirtualAlloc commit");
    }
}

void platform_memory_decommit(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

    if (VirtualFree(ptr, size, MEM_DECOMMIT) == 0) {
        platform_memory_fail("VirtualFree decommit");
    }
}

void platform_memory_release(void* ptr, u64 size) {
    (void)size;
    if (ptr == nullptr) {
        return;
    }

    if (VirtualFree(ptr, 0, MEM_RELEASE) == 0) {
        platform_memory_fail("VirtualFree release");
    }
}

#endif
