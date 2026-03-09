#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void os_memory_fail(const char* operation) {
    int error = errno;
    log_fatal("%s failed: %s", operation, strerror(error));
    abort();
}

void* os_memory_reserve(u64 size) {
    void* ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED) {
        os_memory_fail("mmap reserve");
    }
    return ptr;
}

u64 os_memory_page_size() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        log_fatal("sysconf(_SC_PAGESIZE) failed");
        abort();
    }
    return (u64)page_size;
}

void os_memory_commit(void* ptr, u64 size) {
    if (size == 0) return;

    if (mprotect(ptr, size, PROT_READ | PROT_WRITE) != 0) {
        os_memory_fail("mprotect commit");
    }
}

void os_memory_decommit(void* ptr, u64 size) {
    if (size == 0) return;

    if (mprotect(ptr, size, PROT_NONE) != 0) {
        os_memory_fail("mprotect decommit");
    }
    if (madvise(ptr, size, MADV_DONTNEED) != 0) {
        os_memory_fail("madvise decommit");
    }
}

void os_memory_release(void* ptr, u64 size) {
    if (ptr == NULL || size == 0) return;
    if (munmap(ptr, size) != 0) {
        os_memory_fail("munmap release");
    }
}
