#if OS_MAC

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static void pmFailPosix(const char* operation) {
    int error = errno;
    LOG_FATAL("%s failed: %s", operation, strerror(error));
    abort();
}

void* pmReserveMemory(u64 size) {
    void* ptr = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED) {
        pmFailPosix("mmap reserve");
    }
    return ptr;
}

u64 pmGetPageSize(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        LOG_FATAL("sysconf(_SC_PAGESIZE) failed");
        abort();
    }
    return (u64)page_size;
}

void pmCommitMemory(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

    if (mprotect(ptr, size, PROT_READ | PROT_WRITE) != 0) {
        pmFailPosix("mprotect commit");
    }
}

void pmDecommitMemory(void* ptr, u64 size) {
    if (size == 0) {
        return;
    }

    if (mprotect(ptr, size, PROT_NONE) != 0) {
        pmFailPosix("mprotect decommit");
    }
    if (madvise(ptr, size, MADV_DONTNEED) != 0) {
        pmFailPosix("madvise decommit");
    }
}

void pmReleaseMemory(void* ptr, u64 size) {
    if (ptr == nullptr || size == 0) {
        return;
    }
    if (munmap(ptr, size) != 0) {
        pmFailPosix("munmap release");
    }
}

#endif
