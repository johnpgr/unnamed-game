#include "platform/linux/internal.h"

PlatformErrorCode pfsCopyFile(
    String source,
    String dest,
    bool overwrite_if_exists
) {
    Arena* scratch = pGetScratchLinux();
    u64 scratch_mark = scratch->mark();
    const char* source_path = source.toCStr(scratch);
    const char* dest_path = dest.toCStr(scratch);

    int source_fd = open(source_path, O_RDONLY);
    if (source_fd < 0) {
        PlatformErrorCode result = pfsGetCopyErrorLinux(errno);
        scratch->restore(scratch_mark);
        return result;
    }

    struct stat source_stat = {};
    if (fstat(source_fd, &source_stat) != 0) {
        PlatformErrorCode result = pfsGetCopyErrorLinux(errno);
        close(source_fd);
        scratch->restore(scratch_mark);
        return result;
    }

    int dest_flags = O_WRONLY | O_CREAT;
    dest_flags |= overwrite_if_exists ? O_TRUNC : O_EXCL;

    int dest_fd = open(dest_path, dest_flags, source_stat.st_mode);
    if (dest_fd < 0) {
        PlatformErrorCode result = pfsGetCopyErrorLinux(errno);
        close(source_fd);
        scratch->restore(scratch_mark);
        return result;
    }

    PlatformErrorCode result = PlatformErrorCode::PLATFORM_ERROR_SUCCESS;
    char buffer[64 * KB];
    for (;;) {
        ssize_t bytes_read = read(source_fd, buffer, sizeof(buffer));
        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            result = pfsGetCopyErrorLinux(errno);
            break;
        }

        ssize_t total_written = 0;
        while (total_written < bytes_read) {
            ssize_t bytes_written = write(
                dest_fd,
                buffer + total_written,
                (size_t)(bytes_read - total_written)
            );
            if (bytes_written < 0) {
                result = pfsGetCopyErrorLinux(errno);
                break;
            }
            total_written += bytes_written;
        }

        if (result != PlatformErrorCode::PLATFORM_ERROR_SUCCESS) {
            break;
        }
    }

    close(dest_fd);
    close(source_fd);
    scratch->restore(scratch_mark);
    return result;
}
