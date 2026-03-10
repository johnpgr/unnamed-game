#include "platform/macos/internal.h"

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

    String filename = strConcat(
        arena,
        strConcat(arena, pdlGetLibraryPrefix(), name),
        pdlGetLibraryExtension()
    );

    const char* filename_cstr = (const char*)filename.str;
    Arena* scratch = pGetScratchMacOS();
    u64 scratch_mark = scratch->mark();
    char executable_dir[PATH_MAX] = {};
    bool have_executable_dir =
        pGetExecutableDirMacOS(executable_dir, sizeof(executable_dir));
    char cwd[PATH_MAX] = {};
    bool have_cwd = getcwd(cwd, sizeof(cwd)) != nullptr;
    void* handle = nullptr;

    if (have_executable_dir) {
        String resolved_path =
            String::fmt(scratch, "%s/%s", executable_dir, filename_cstr);
        if (pdlTryLoadLibraryMacOS((const char*)resolved_path.str, &handle)) {
            out_library->filename = String::copy(arena, resolved_path);
        }
    }

    if (!handle && have_cwd) {
        String resolved_path =
            String::fmt(scratch, "%s/%s", cwd, filename_cstr);
        if (pdlTryLoadLibraryMacOS((const char*)resolved_path.str, &handle)) {
            out_library->filename = String::copy(arena, resolved_path);
        }
    }

    if (!handle && pdlTryLoadLibraryMacOS(filename_cstr, &handle)) {
        out_library->filename = String::copy(arena, filename);
    }

    if (!handle) {
        scratch->restore(scratch_mark);
        *out_library = {};
        return false;
    }

    out_library->arena = arena;
    out_library->name = String::copy(arena, name);
    out_library->internal_data = handle;
    out_library->internal_data_size = sizeof(void*);
    out_library->watch_id = 0;
    out_library->functions = ArrayList<DynLibFn>::make(arena);
    scratch->restore(scratch_mark);
    return true;
}

bool pdlUnloadLibrary(DynLib* library) {
    if (!library || !library->internal_data) {
        return false;
    }

    if (dlclose(library->internal_data) != 0) {
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

    Arena* scratch = pGetScratchMacOS();
    u64 scratch_mark = scratch->mark();
    const char* symbol_name = name.toCStr(scratch);
    dlerror();
    void* symbol = dlsym(library->internal_data, symbol_name);
    const char* error = dlerror();
    if (error != nullptr || !symbol) {
        scratch->restore(scratch_mark);
        return nullptr;
    }

    DynLibFn function = {};
    function.name = String::copy(library->arena, name);
    function.pfn = symbol;
    library->functions.push(function);
    scratch->restore(scratch_mark);
    return symbol;
}

String pdlGetLibraryExtension(void) {
    return String::lit(".dylib");
}

String pdlGetLibraryPrefix(void) {
    return String::lit("lib");
}
