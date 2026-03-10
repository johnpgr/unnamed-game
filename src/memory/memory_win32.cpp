#if OS_WINDOWS

#include <windows.h>

internal void pmFailWin32(const char* operation) {
    DWORD error = GetLastError();
    LOG_FATAL("%s failed with error %lu", operation, (unsigned long)error);
    abort();
}

void* pmReserveMemory(u64 size) {
    void* ptr = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (ptr == nullptr) {
        pmFailWin32("VirtualAlloc reserve");
    }
    return ptr;
}

u64 pmGetPageSize(void) {
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    return (u64)system_info.dwPageSize;
}

void pmCommitMemory(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if (result == nullptr) {
        pmFailWin32("VirtualAlloc commit");
    }
}

void pmDecommitMemory(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

    if (VirtualFree(ptr, size, MEM_DECOMMIT) == 0) {
        pmFailWin32("VirtualFree decommit");
    }
}

void pmReleaseMemory(void* ptr, u64 size) {
    (void)size;
    if (ptr == nullptr) {
        return;
    }

    if (VirtualFree(ptr, 0, MEM_RELEASE) == 0) {
        pmFailWin32("VirtualFree release");
    }
}

#endif
