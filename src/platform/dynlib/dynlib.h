#pragma once

struct DynLibFn {
    String name;
    void* pfn;
};

struct DynLib {
    Arena* arena;
    String name;
    String filename;
    u64 internal_data_size;
    void* internal_data;
    u32 watch_id;

    ArrayList<DynLibFn> functions;
};

bool pdlLoadLibrary(
    Arena* arena,
    String name,
    DynLib* out_library
);

template <u64 N>
inline bool pdlLoadLibrary(
    Arena* arena,
    const char (&name)[N],
    DynLib* out_library
) {
    return pdlLoadLibrary(
        arena,
        {(const u8*)name, N - 1},
        out_library
    );
}

inline bool pdlLoadLibrary(
    Arena* arena,
    const char* name,
    DynLib* out_library
) {
    return pdlLoadLibrary(
        arena,
        String::fromCStr(name),
        out_library
    );
}

bool pdlUnloadLibrary(DynLib* library);
void* pdlLoadFunction(String name, DynLib* library);

template <u64 N>
inline void* pdlLoadFunction(
    const char (&name)[N],
    DynLib* library
) {
    return pdlLoadFunction(
        {(const u8*)name, N - 1},
        library
    );
}

inline void* pdlLoadFunction(
    const char* name,
    DynLib* library
) {
    return pdlLoadFunction(
        String::fromCStr(name),
        library
    );
}

String pdlGetLibraryExtension(void);
String pdlGetLibraryPrefix(void);
