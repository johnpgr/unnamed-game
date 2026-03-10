#pragma once

#include <windows.h>
#include <vulkan/vulkan_win32.h>

struct Win32PlatformState {
    HINSTANCE instance;
    ATOM window_class;
    HWND window;
    int width;
    int height;
    bool class_registered;
    bool initialized;
    bool should_close;
    bool visible;
    bool resizable;
};

internal Win32PlatformState win32_state = {};
internal Arena win32_scratch_arena = {};
internal bool win32_scratch_initialized = false;

internal void pFail(const char* message) {
    LOG_FATAL("%s", message);
    abort();
}

internal Arena* pGetScratchWin32(void) {
    if (!win32_scratch_initialized) {
        win32_scratch_arena = Arena::make();
        win32_scratch_initialized = true;
    }
    return &win32_scratch_arena;
}

internal wchar_t* pToWideStringWin32(Arena* scratch, String text) {
    int utf8_length = (int)text.size;
    int wide_length = MultiByteToWideChar(
        CP_UTF8,
        0,
        (const char*)text.str,
        utf8_length,
        nullptr,
        0
    );
    if (wide_length <= 0) {
        return nullptr;
    }

    wchar_t* result =
        scratch->push<wchar_t>((u64)wide_length + 1, alignof(wchar_t));

    int written = MultiByteToWideChar(
        CP_UTF8,
        0,
        (const char*)text.str,
        utf8_length,
        result,
        wide_length
    );
    if (written != wide_length) {
        return nullptr;
    }

    result[wide_length] = 0;
    return result;
}

internal bool pGetExecutableDirWin32(wchar_t* buffer, DWORD buffer_count) {
    DWORD length = GetModuleFileNameW(nullptr, buffer, buffer_count);
    if (length == 0 || length == buffer_count) {
        return false;
    }

    wchar_t* slash = wcsrchr(buffer, L'\\');
    if (!slash) {
        return false;
    }
    *slash = 0;
    return true;
}

internal bool pdlTryLoadLibraryWin32(
    const wchar_t* path,
    HMODULE* out_library
) {
    assert(path != nullptr, "Library path must not be null!");
    assert(out_library != nullptr, "Output library must not be null!");

    HMODULE library = LoadLibraryW(path);
    if (!library) {
        return false;
    }

    *out_library = library;
    return true;
}

internal DWORD pwGetWindowStyleWin32(void) {
    DWORD style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;
    if (win32_state.resizable) {
        style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
    }
    return style;
}

internal LRESULT CALLBACK
pwWindowProcWin32(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CLOSE:
            win32_state.should_close = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            win32_state.window = nullptr;
            win32_state.visible = false;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            win32_state.width = (int)LOWORD(lparam);
            win32_state.height = (int)HIWORD(lparam);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

internal bool pwRegisterWindowClassWin32(void) {
    if (win32_state.class_registered) {
        return true;
    }

    win32_state.instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = pwWindowProcWin32;
    window_class.hInstance = win32_state.instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszClassName = L"cpp_gaming_window_class";

    win32_state.window_class = RegisterClassExW(&window_class);
    win32_state.class_registered = win32_state.window_class != 0;
    return win32_state.class_registered;
}

internal String
pFromWidePathWin32(Arena* arena, Arena* scratch, const wchar_t* path) {
    int utf8_length =
        WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    assert(utf8_length > 0, "Wide to UTF-8 conversion failed!");

    char* buffer = scratch->push<char>((u64)utf8_length, alignof(char));

    int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        path,
        -1,
        buffer,
        utf8_length,
        nullptr,
        nullptr
    );
    assert(written == utf8_length, "Wide to UTF-8 conversion failed!");

    return String::copy(arena, String::fromCStr(buffer));
}

internal PlatformErrorCode pfsGetCopyErrorWin32(DWORD error) {
    switch (error) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return PlatformErrorCode::PLATFORM_ERROR_FILE_NOT_FOUND;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return PlatformErrorCode::PLATFORM_ERROR_FILE_LOCKED;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
            return PlatformErrorCode::PLATFORM_ERROR_FILE_EXISTS;
        default:
            return PlatformErrorCode::PLATFORM_ERROR_UNKNOWN;
    }
}
