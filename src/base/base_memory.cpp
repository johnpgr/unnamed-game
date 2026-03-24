#include <errno.h>
#include <string.h>

#include "base/base_memory.h"

#if OS_WINDOWS
#include <windows.h>
#else
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

internal void fatal_system_call(char const* operation) {
    ASSERT(operation != nullptr, "Operation name must not be null!");

#if OS_WINDOWS
    DWORD error = GetLastError();
    if(error != 0) {
        LOG_FATAL("%s failed with error %lu", operation, (unsigned long)error);
    } else {
        LOG_FATAL("%s failed", operation);
    }
#else
    int error = errno;
    if(error != 0) {
        LOG_FATAL("%s failed: %s", operation, strerror(error));
    } else {
        LOG_FATAL("%s failed", operation);
    }
#endif

    abort();
}

void* reserve_system_memory(u64 size) {
#if OS_WINDOWS
    void* ptr = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if(ptr == nullptr) {
        fatal_system_call("VirtualAlloc reserve");
    }
    return ptr;
#else
    int map_anon_flag =
#if OS_MAC
        MAP_ANON;
#else
        MAP_ANONYMOUS;
#endif
    void* ptr =
        mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | map_anon_flag, -1, 0);
    if(ptr == MAP_FAILED) {
        fatal_system_call("mmap reserve");
    }
    return ptr;
#endif
}

u64 get_system_page_size(void) {
#if OS_WINDOWS
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    return (u64)system_info.dwPageSize;
#else
    long page_size = sysconf(_SC_PAGESIZE);
    if(page_size <= 0) {
        LOG_FATAL("sysconf(_SC_PAGESIZE) failed");
        abort();
    }
    return (u64)page_size;
#endif
}

void commit_system_memory(void* ptr, u64 size) {
    if(size == 0) {
        return;
    }

#if OS_WINDOWS
    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if(result == nullptr) {
        fatal_system_call("VirtualAlloc commit");
    }
#else
    if(mprotect(ptr, size, PROT_READ | PROT_WRITE) != 0) {
        fatal_system_call("mprotect commit");
    }
#endif
}

void decommit_system_memory(void* ptr, u64 size) {
    if(size == 0) {
        return;
    }

#if OS_WINDOWS
    if(VirtualFree(ptr, size, MEM_DECOMMIT) == 0) {
        fatal_system_call("VirtualFree decommit");
    }
#else
    if(mprotect(ptr, size, PROT_NONE) != 0) {
        fatal_system_call("mprotect decommit");
    }
    if(madvise(ptr, size, MADV_DONTNEED) != 0) {
        fatal_system_call("madvise decommit");
    }
#endif
}

void release_system_memory(void* ptr, u64 size) {
#if OS_WINDOWS
    (void)size;
    if(ptr == nullptr) {
        return;
    }

    if(VirtualFree(ptr, 0, MEM_RELEASE) == 0) {
        fatal_system_call("VirtualFree release");
    }
#else
    if(ptr == nullptr || size == 0) {
        return;
    }

    if(munmap(ptr, size) != 0) {
        fatal_system_call("munmap release");
    }
#endif
}
