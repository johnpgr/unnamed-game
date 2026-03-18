#pragma once

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "base/core.h"

#if OS_WINDOWS
#include <windows.h>
#else
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

inline void FatalSystemCall(const char* operation) {
    assert(operation != nullptr, "Operation name must not be null!");

#if OS_WINDOWS
    DWORD error = GetLastError();
    if (error != 0) {
        LOG_FATAL("%s failed with error %lu", operation, (unsigned long)error);
    } else {
        LOG_FATAL("%s failed", operation);
    }
#else
    int error = errno;
    if (error != 0) {
        LOG_FATAL("%s failed: %s", operation, strerror(error));
    } else {
        LOG_FATAL("%s failed", operation);
    }
#endif

    abort();
}

inline void* ReserveSystemMemory(u64 size) {
#if OS_WINDOWS
    void* ptr = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (ptr == nullptr) {
        FatalSystemCall("VirtualAlloc reserve");
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
    if (ptr == MAP_FAILED) {
        FatalSystemCall("mmap reserve");
    }
    return ptr;
#endif
}

inline u64 GetSystemPageSize(void) {
#if OS_WINDOWS
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    return (u64)system_info.dwPageSize;
#else
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        LOG_FATAL("sysconf(_SC_PAGESIZE) failed");
        abort();
    }
    return (u64)page_size;
#endif
}

inline void CommitSystemMemory(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

#if OS_WINDOWS
    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if (result == nullptr) {
        FatalSystemCall("VirtualAlloc commit");
    }
#else
    if (mprotect(ptr, size, PROT_READ | PROT_WRITE) != 0) {
        FatalSystemCall("mprotect commit");
    }
#endif
}

inline void DecommitSystemMemory(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

#if OS_WINDOWS
    if (VirtualFree(ptr, size, MEM_DECOMMIT) == 0) {
        FatalSystemCall("VirtualFree decommit");
    }
#else
    if (mprotect(ptr, size, PROT_NONE) != 0) {
        FatalSystemCall("mprotect decommit");
    }
    if (madvise(ptr, size, MADV_DONTNEED) != 0) {
        FatalSystemCall("madvise decommit");
    }
#endif
}

inline void ReleaseSystemMemory(void* ptr, u64 size) {
#if OS_WINDOWS
    (void)size;
    if (ptr == nullptr) {
        return;
    }

    if (VirtualFree(ptr, 0, MEM_RELEASE) == 0) {
        FatalSystemCall("VirtualFree release");
    }
#else
    if (ptr == nullptr || size == 0) {
        return;
    }

    if (munmap(ptr, size) != 0) {
        FatalSystemCall("munmap release");
    }
#endif
}
