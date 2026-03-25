#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "os/os_mod.h"

internal void os_fatal_system_call(char const* operation) {
    ASSERT(operation != nullptr, "Operation name must not be null!");

    int error = errno;
    if(error != 0) {
        LOG_FATAL("%s failed: %s", operation, strerror(error));
    } else {
        LOG_FATAL("%s failed", operation);
    }

    abort();
}

void* reserve_system_memory(u64 size) {
    int map_anon_flag =
#if OS_MAC
        MAP_ANON;
#else
        MAP_ANONYMOUS;
#endif
    void* ptr =
        mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | map_anon_flag, -1, 0);
    if(ptr == MAP_FAILED) {
        os_fatal_system_call("mmap reserve");
    }
    return ptr;
}

u64 get_system_page_size(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    if(page_size <= 0) {
        LOG_FATAL("sysconf(_SC_PAGESIZE) failed");
        abort();
    }
    return (u64)page_size;
}

void commit_system_memory(void* ptr, u64 size) {
    if(size == 0) {
        return;
    }

    if(mprotect(ptr, size, PROT_READ | PROT_WRITE) != 0) {
        os_fatal_system_call("mprotect commit");
    }
}

void decommit_system_memory(void* ptr, u64 size) {
    if(size == 0) {
        return;
    }

    if(mprotect(ptr, size, PROT_NONE) != 0) {
        os_fatal_system_call("mprotect decommit");
    }
    if(madvise(ptr, size, MADV_DONTNEED) != 0) {
        os_fatal_system_call("madvise decommit");
    }
}

void release_system_memory(void* ptr, u64 size) {
    if(ptr == nullptr || size == 0) {
        return;
    }

    if(munmap(ptr, size) != 0) {
        os_fatal_system_call("munmap release");
    }
}
