#include "platform/win32/internal.h"

PlatformErrorCode pfsCopyFile(
    String source,
    String dest,
    bool overwrite_if_exists
) {
    Arena* scratch = pGetScratchWin32();
    u64 scratch_mark = scratch->mark();
    wchar_t* wide_source = pToWideStringWin32(scratch, source);
    wchar_t* wide_dest = pToWideStringWin32(scratch, dest);
    if (!wide_source || !wide_dest) {
        scratch->restore(scratch_mark);
        return PlatformErrorCode::PLATFORM_ERROR_UNKNOWN;
    }

    BOOL copied =
        CopyFileW(wide_source, wide_dest, overwrite_if_exists ? FALSE : TRUE);
    DWORD error = copied != 0 ? 0 : GetLastError();
    scratch->restore(scratch_mark);

    if (copied != 0) {
        return PlatformErrorCode::PLATFORM_ERROR_SUCCESS;
    }

    return pfsGetCopyErrorWin32(error);
}
