#pragma once

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xcb/xcb.h>

struct LinuxPlatformState {
    xcb_connection_t* connection;
    xcb_screen_t* screen;
    xcb_window_t window;
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_window;
    xcb_atom_t net_wm_name;
    xcb_atom_t utf8_string;
    int width;
    int height;
    bool initialized;
    bool should_close;
    bool visible;
    bool resizable;
};

internal LinuxPlatformState linux_state = {};
internal Arena linux_scratch_arena = {};
internal bool linux_scratch_initialized = false;

internal xcb_screen_t* pwGetDefaultScreenLinux(xcb_connection_t* connection) {
    const xcb_setup_t* setup = xcb_get_setup(connection);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    if (it.rem == 0 || !it.data) {
        return nullptr;
    }
    return it.data;
}

internal xcb_atom_t pwInternAtomLinux(const char* name) {
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(linux_state.connection, 0, (u16)strlen(name), name);
    xcb_intern_atom_reply_t* reply =
        xcb_intern_atom_reply(linux_state.connection, cookie, nullptr);
    if (!reply) {
        return XCB_ATOM_NONE;
    }

    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

internal void pwApplyWindowTitleLinux(String title) {
    if (!linux_state.initialized || linux_state.window == XCB_WINDOW_NONE) {
        return;
    }

    String effective_title = title.size > 0 ? title : String::lit("cpp-gaming");

    xcb_change_property(
        linux_state.connection,
        XCB_PROP_MODE_REPLACE,
        linux_state.window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        (u32)effective_title.size,
        effective_title.str
    );

    if (linux_state.net_wm_name != XCB_ATOM_NONE &&
        linux_state.utf8_string != XCB_ATOM_NONE) {
        xcb_change_property(
            linux_state.connection,
            XCB_PROP_MODE_REPLACE,
            linux_state.window,
            linux_state.net_wm_name,
            linux_state.utf8_string,
            8,
            (u32)effective_title.size,
            effective_title.str
        );
    }
}

internal void pwDestroyNativeWindowLinux(void) {
    if (linux_state.connection != nullptr &&
        linux_state.window != XCB_WINDOW_NONE) {
        xcb_destroy_window(linux_state.connection, linux_state.window);
        xcb_flush(linux_state.connection);
        linux_state.window = XCB_WINDOW_NONE;
    }
}

internal Arena* pGetScratchLinux(void) {
    if (!linux_scratch_initialized) {
        linux_scratch_arena = Arena::make();
        linux_scratch_initialized = true;
    }
    return &linux_scratch_arena;
}

internal bool pGetExecutableDirLinux(char* buffer, usize buffer_size) {
    assert(buffer != nullptr, "Buffer must not be null!");
    assert(buffer_size > 0, "Buffer size must be non-zero!");

    ssize_t size = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if (size <= 0) {
        return false;
    }

    buffer[size] = 0;
    char* slash = strrchr(buffer, '/');
    if (!slash) {
        return false;
    }
    *slash = 0;
    return true;
}

internal bool pdlTryLoadLibraryLinux(const char* path, void** out_handle) {
    assert(path != nullptr, "Library path must not be null!");
    assert(out_handle != nullptr, "Output handle must not be null!");

    dlerror();
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return false;
    }

    *out_handle = handle;
    return true;
}

internal PlatformErrorCode pfsGetCopyErrorLinux(int error) {
    switch (error) {
        case 0:
            return PlatformErrorCode::PLATFORM_ERROR_SUCCESS;
        case ENOENT:
            return PlatformErrorCode::PLATFORM_ERROR_FILE_NOT_FOUND;
        case EEXIST:
            return PlatformErrorCode::PLATFORM_ERROR_FILE_EXISTS;
        case EBUSY:
        case ETXTBSY:
            return PlatformErrorCode::PLATFORM_ERROR_FILE_LOCKED;
        default:
            return PlatformErrorCode::PLATFORM_ERROR_UNKNOWN;
    }
}
