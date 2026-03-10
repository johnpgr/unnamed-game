#include "platform/macos/internal.h"

PlatformErrorCode pfsCopyFile(
    String source,
    String dest,
    bool overwrite_if_exists
) {
    Arena* scratch = pGetScratchMacOS();
    u64 scratch_mark = scratch->mark();
    const char* source_path = source.toCStr(scratch);
    const char* dest_path = dest.toCStr(scratch);
    copyfile_flags_t flags = COPYFILE_ALL;
    if (!overwrite_if_exists) {
        flags |= COPYFILE_EXCL;
    }

    int copied = copyfile(source_path, dest_path, nullptr, flags);
    int error = copied == 0 ? 0 : errno;
    scratch->restore(scratch_mark);
    return pfsGetCopyErrorMacOS(error);
}
