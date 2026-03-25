#include <windows.h>

#include "os/os_mod.h"

internal void os_fatal_system_call(char const* operation) {
    ASSERT(operation != nullptr, "Operation name must not be null!");

    DWORD error = GetLastError();
    if(error != 0) {
        LOG_FATAL("%s failed with error %lu", operation, (unsigned long)error);
    } else {
        LOG_FATAL("%s failed", operation);
    }

    abort();
}

void* reserve_system_memory(u64 size) {
    void* ptr = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if(ptr == nullptr) {
        os_fatal_system_call("VirtualAlloc reserve");
    }
    return ptr;
}

u64 get_system_page_size(void) {
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    return (u64)system_info.dwPageSize;
}

void commit_system_memory(void* ptr, u64 size) {
    if(size == 0) {
        return;
    }

    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if(result == nullptr) {
        os_fatal_system_call("VirtualAlloc commit");
    }
}

void decommit_system_memory(void* ptr, u64 size) {
    if(size == 0) {
        return;
    }

    if(VirtualFree(ptr, size, MEM_DECOMMIT) == 0) {
        os_fatal_system_call("VirtualFree decommit");
    }
}

void release_system_memory(void* ptr, u64 size) {
    (void)size;
    if(ptr == nullptr) {
        return;
    }

    if(VirtualFree(ptr, 0, MEM_RELEASE) == 0) {
        os_fatal_system_call("VirtualFree release");
    }
}
