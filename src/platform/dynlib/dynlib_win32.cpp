#include "platform/win32/internal.h"

bool pdlLoadLibrary(
    Arena* arena,
    String name,
    DynLib* out_library
) {
    if (!arena || !out_library || name.size == 0) {
        return false;
    }

    *out_library = {};
    out_library->arena = arena;

    Arena* scratch = pGetScratchWin32();
    u64 scratch_mark = scratch->mark();
    String filename = strConcat(
        arena,
        strConcat(arena, pdlGetLibraryPrefix(), name),
        pdlGetLibraryExtension()
    );
    wchar_t* wide_filename = pToWideStringWin32(scratch, filename);
    if (!wide_filename) {
        scratch->restore(scratch_mark);
        *out_library = {};
        return false;
    }

    HMODULE module = nullptr;
    wchar_t path_buffer[MAX_PATH] = {};
    if (pGetExecutableDirWin32(path_buffer, MAX_PATH)) {
        wchar_t candidate[MAX_PATH] = {};
        _snwprintf_s(
            candidate,
            MAX_PATH,
            _TRUNCATE,
            L"%ls\\%ls",
            path_buffer,
            wide_filename
        );
        if (pdlTryLoadLibraryWin32(candidate, &module)) {
            out_library->filename =
                pFromWidePathWin32(arena, scratch, candidate);
        }
    }

    if (!module) {
        DWORD cwd_length = GetCurrentDirectoryW(MAX_PATH, path_buffer);
        if (cwd_length > 0 && cwd_length < MAX_PATH) {
            wchar_t candidate[MAX_PATH] = {};
            _snwprintf_s(
                candidate,
                MAX_PATH,
                _TRUNCATE,
                L"%ls\\%ls",
                path_buffer,
                wide_filename
            );
            if (pdlTryLoadLibraryWin32(candidate, &module)) {
                out_library->filename =
                    pFromWidePathWin32(arena, scratch, candidate);
            }
        }
    }

    if (!module && pdlTryLoadLibraryWin32(wide_filename, &module)) {
        out_library->filename = String::copy(arena, filename);
    }

    if (!module) {
        scratch->restore(scratch_mark);
        *out_library = {};
        return false;
    }

    out_library->arena = arena;
    out_library->name = String::copy(arena, name);
    out_library->internal_data = module;
    out_library->internal_data_size = sizeof(HMODULE);
    out_library->watch_id = 0;
    out_library->functions = ArrayList<DynLibFn>::make(arena);
    scratch->restore(scratch_mark);
    return true;
}

bool pdlUnloadLibrary(DynLib* library) {
    if (!library || !library->internal_data) {
        return false;
    }

    if (FreeLibrary((HMODULE)library->internal_data) == 0) {
        return false;
    }

    *library = {};
    return true;
}

void* pdlLoadFunction(String name, DynLib* library) {
    if (!library || !library->internal_data || !library->arena ||
        name.size == 0) {
        return nullptr;
    }

    for (ArrayListNode<DynLibFn>* node = library->functions.first;
         node != nullptr;
         node = node->next) {
        if (node->value.name.equals(name)) {
            return node->value.pfn;
        }
    }

    Arena* scratch = pGetScratchWin32();
    u64 scratch_mark = scratch->mark();
    const char* symbol_name = name.toCStr(scratch);
    FARPROC symbol =
        GetProcAddress((HMODULE)library->internal_data, symbol_name);
    if (!symbol) {
        scratch->restore(scratch_mark);
        return nullptr;
    }

    DynLibFn function = {};
    function.name = String::copy(library->arena, name);
    function.pfn = (void*)symbol;
    library->functions.push(function);
    scratch->restore(scratch_mark);
    return function.pfn;
}

String pdlGetLibraryExtension(void) {
    return String::lit(".dll");
}

String pdlGetLibraryPrefix(void) {
    return String::lit("");
}
