#pragma once

PlatformErrorCode pfsCopyFile(
    String source,
    String dest,
    bool overwrite_if_exists
);

template <u64 SourceN, u64 DestN>
inline PlatformErrorCode pfsCopyFile(
    const char (&source)[SourceN],
    const char (&dest)[DestN],
    bool overwrite_if_exists
) {
    return pfsCopyFile(
        {(const u8*)source, SourceN - 1},
        {(const u8*)dest, DestN - 1},
        overwrite_if_exists
    );
}

template <u64 SourceN>
inline PlatformErrorCode pfsCopyFile(
    const char (&source)[SourceN],
    const char* dest,
    bool overwrite_if_exists
) {
    return pfsCopyFile(
        {(const u8*)source, SourceN - 1},
        String::fromCStr(dest),
        overwrite_if_exists
    );
}

template <u64 DestN>
inline PlatformErrorCode pfsCopyFile(
    const char* source,
    const char (&dest)[DestN],
    bool overwrite_if_exists
) {
    return pfsCopyFile(
        String::fromCStr(source),
        {(const u8*)dest, DestN - 1},
        overwrite_if_exists
    );
}

inline PlatformErrorCode pfsCopyFile(
    const char* source,
    const char* dest,
    bool overwrite_if_exists
) {
    return pfsCopyFile(
        String::fromCStr(source),
        String::fromCStr(dest),
        overwrite_if_exists
    );
}
