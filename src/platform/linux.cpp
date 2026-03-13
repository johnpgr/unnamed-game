#if OS_LINUX

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>

namespace Platform {

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

void Fail(const char* message) {
    int error = errno;
    if (error != 0) {
        LOG_FATAL("%s failed: %s", message, strerror(error));
    } else {
        LOG_FATAL("%s", message);
    }
    abort();
}

internal xcb_screen_t* GetDefaultScreenLinux(xcb_connection_t* connection) {
    const xcb_setup_t* setup = xcb_get_setup(connection);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    if (it.rem == 0 || !it.data) {
        return nullptr;
    }
    return it.data;
}

internal xcb_atom_t InternAtomLinux(const char* name) {
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(linux_state.connection, 0, (u16)strlen(name), name);
    xcb_intern_atom_reply_t* reply =
        xcb_intern_atom_reply(linux_state.connection, cookie, nullptr);
    if (!reply) {
        return XCB_ATOM_NONE;
    }
    DEFER {
        free(reply);
    };

    return reply->atom;
}

internal void ApplyWindowTitleLinux(String title) {
    if (!linux_state.initialized || linux_state.window == XCB_WINDOW_NONE) {
        return;
    }

    String effective_title =
        title.size > 0 ? title : String::lit("cpp-gaming");

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

internal void DestroyNativeWindowLinux(void) {
    if (linux_state.connection != nullptr &&
        linux_state.window != XCB_WINDOW_NONE) {
        xcb_destroy_window(linux_state.connection, linux_state.window);
        xcb_flush(linux_state.connection);
        linux_state.window = XCB_WINDOW_NONE;
    }
}

internal Arena* GetScratchLinux(void) {
    if (!linux_scratch_initialized) {
        linux_scratch_arena = CreateArena();
        linux_scratch_initialized = true;
    }
    return &linux_scratch_arena;
}

internal bool GetExecutableDirLinux(char* buffer, usize buffer_size) {
    ASSERT(buffer != nullptr, "Buffer must not be null!");
    ASSERT(buffer_size > 0, "Buffer size must be non-zero!");

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

internal bool TryLoadDynamicLibraryLinux(const char* path, void** out_handle) {
    ASSERT(path != nullptr, "Library path must not be null!");
    ASSERT(out_handle != nullptr, "Output handle must not be null!");

    dlerror();
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return false;
    }

    *out_handle = handle;
    return true;
}

internal ErrorCode GetCopyErrorLinux(int error) {
    switch (error) {
        case 0:
            return ErrorCode::SUCCESS;
        case ENOENT:
            return ErrorCode::FILE_NOT_FOUND;
        case EEXIST:
            return ErrorCode::FILE_EXISTS;
        case EBUSY:
        case ETXTBSY:
            return ErrorCode::FILE_LOCKED;
        default:
            return ErrorCode::UNKNOWN;
    }
}

void CreateWindow(String title, int width, int height) {
    if (linux_state.initialized) {
        DestroyWindow();
    }

    linux_state.connection = xcb_connect(nullptr, nullptr);
    if (!linux_state.connection ||
        xcb_connection_has_error(linux_state.connection) != 0) {
        LOG_FATAL("Failed to connect to the X server.");
        abort();
    }

    linux_state.screen = GetDefaultScreenLinux(linux_state.connection);
    if (!linux_state.screen) {
        xcb_disconnect(linux_state.connection);
        linux_state = {};
        LOG_FATAL("Failed to acquire the default X screen.");
        abort();
    }

    linux_state.window = xcb_generate_id(linux_state.connection);
    linux_state.width = width;
    linux_state.height = height;
    linux_state.resizable = true;

    u32 event_mask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    u32 value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    u32 value_list[] = {linux_state.screen->black_pixel, event_mask};

    xcb_void_cookie_t create_cookie = xcb_create_window_checked(
        linux_state.connection,
        XCB_COPY_FROM_PARENT,
        linux_state.window,
        linux_state.screen->root,
        0,
        0,
        (u16)width,
        (u16)height,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        linux_state.screen->root_visual,
        value_mask,
        value_list
    );

    xcb_generic_error_t* create_error =
        xcb_request_check(linux_state.connection, create_cookie);
    if (create_error) {
        free(create_error);
        xcb_disconnect(linux_state.connection);
        linux_state = {};
        LOG_FATAL("Failed to create the XCB window.");
        abort();
    }

    linux_state.wm_protocols = InternAtomLinux("WM_PROTOCOLS");
    linux_state.wm_delete_window = InternAtomLinux("WM_DELETE_WINDOW");
    linux_state.net_wm_name = InternAtomLinux("_NET_WM_NAME");
    linux_state.utf8_string = InternAtomLinux("UTF8_STRING");

    if (linux_state.wm_protocols != XCB_ATOM_NONE &&
        linux_state.wm_delete_window != XCB_ATOM_NONE) {
        xcb_change_property(
            linux_state.connection,
            XCB_PROP_MODE_REPLACE,
            linux_state.window,
            linux_state.wm_protocols,
            XCB_ATOM_ATOM,
            32,
            1,
            &linux_state.wm_delete_window
        );
    }

    ApplyWindowTitleLinux(title);
    xcb_flush(linux_state.connection);

    linux_state.should_close = false;
    linux_state.visible = false;
    linux_state.initialized = true;
}

void DestroyWindow(void) {
    if (!linux_state.initialized) {
        return;
    }

    DestroyNativeWindowLinux();
    if (linux_state.connection != nullptr) {
        xcb_disconnect(linux_state.connection);
    }
    linux_state = {};
}

bool ShouldWindowClose(void) {
    return linux_state.should_close;
}

void PollEvents(void) {
    if (!linux_state.initialized || !linux_state.connection) {
        return;
    }

    xcb_generic_event_t* event = nullptr;
    while ((event = xcb_poll_for_event(linux_state.connection)) != nullptr) {
        u8 response_type = (u8)(event->response_type & 0x7f);
        switch (response_type) {
            case XCB_CLIENT_MESSAGE: {
                xcb_client_message_event_t* client_message =
                    (xcb_client_message_event_t*)event;
                if (client_message->type == linux_state.wm_protocols &&
                    client_message->data.data32[0] ==
                        linux_state.wm_delete_window) {
                    linux_state.should_close = true;
                }
            } break;
            case XCB_CONFIGURE_NOTIFY: {
                xcb_configure_notify_event_t* configure =
                    (xcb_configure_notify_event_t*)event;
                linux_state.width = configure->width;
                linux_state.height = configure->height;
            } break;
            case XCB_DESTROY_NOTIFY:
                linux_state.should_close = true;
                break;
            default:
                break;
        }

        free(event);
    }
}

void SetWindowTitle(String title) {
    ApplyWindowTitleLinux(title);
    if (linux_state.connection != nullptr) {
        xcb_flush(linux_state.connection);
    }
}

void GetWindowSize(int* width, int* height) {
    if (width) {
        *width = linux_state.width;
    }
    if (height) {
        *height = linux_state.height;
    }
}

void SetWindowSize(int width, int height) {
    if (!linux_state.initialized || linux_state.window == XCB_WINDOW_NONE) {
        return;
    }

    u32 values[] = {(u32)width, (u32)height};
    xcb_configure_window(
        linux_state.connection,
        linux_state.window,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
        values
    );
    xcb_flush(linux_state.connection);

    linux_state.width = width;
    linux_state.height = height;
}

void SetWindowResizable(bool resizable) {
    linux_state.resizable = resizable;
}

void PresentWindow(void) {
    if (linux_state.connection != nullptr) {
        xcb_flush(linux_state.connection);
    }
}

void ShowWindow(void) {
    if (!linux_state.initialized || linux_state.visible ||
        linux_state.window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_map_window(linux_state.connection, linux_state.window);
    xcb_flush(linux_state.connection);
    linux_state.visible = true;
}

void CreateAudio(void) {
}

void DestroyAudio(void) {
}

void UpdateAudioBuffer(void) {
}

void SetAudioVolume(f32 volume) {
    (void)volume;
}

bool LoadDynamicLibrary(Arena* arena, String name, DynamicLibrary* out_library) {
    if (!arena || !out_library || name.size == 0) {
        return false;
    }

    *out_library = {};
    out_library->arena = arena;

    String filename = StringConcat(
        arena,
        StringConcat(arena, GetDynamicLibraryPrefix(), name),
        GetDynamicLibraryExtension()
    );

    Arena* scratch = GetScratchLinux();
    u64 scratch_mark = scratch->mark();
    DEFER {
        scratch->restore(scratch_mark);
    };

    const char* filename_cstr = filename.toCstr(scratch);
    char executable_dir[PATH_MAX] = {};
    bool have_executable_dir =
        GetExecutableDirLinux(executable_dir, sizeof(executable_dir));
    char cwd[PATH_MAX] = {};
    bool have_cwd = getcwd(cwd, sizeof(cwd)) != nullptr;
    void* handle = nullptr;

    if (have_executable_dir) {
        String resolved_path =
            String::fmt(scratch, "%s/%s", executable_dir, filename_cstr);
        if (TryLoadDynamicLibraryLinux((const char*)resolved_path.str, &handle)) {
            out_library->filename = String::copy(arena, resolved_path);
        }
    }

    if (!handle && have_cwd) {
        String resolved_path = String::fmt(scratch, "%s/%s", cwd, filename_cstr);
        if (TryLoadDynamicLibraryLinux((const char*)resolved_path.str, &handle)) {
            out_library->filename = String::copy(arena, resolved_path);
        }
    }

    if (!handle && TryLoadDynamicLibraryLinux(filename_cstr, &handle)) {
        out_library->filename = String::copy(arena, filename);
    }

    if (!handle) {
        *out_library = {};
        return false;
    }

    out_library->name = String::copy(arena, name);
    out_library->internal_data = handle;
    out_library->internal_data_size = sizeof(void*);
    out_library->watch_id = 0;
    out_library->functions = ArrayList<DynamicLibraryFunction>::make(arena);
    return true;
}

bool UnloadDynamicLibrary(DynamicLibrary* library) {
    if (!library || !library->internal_data) {
        return false;
    }

    if (dlclose(library->internal_data) != 0) {
        return false;
    }

    *library = {};
    return true;
}

void* LoadDynamicFunction(String name, DynamicLibrary* library) {
    if (!library || !library->internal_data || !library->arena ||
        name.size == 0) {
        return nullptr;
    }

    for (ArrayListNode<DynamicLibraryFunction>* node = library->functions.first;
         node != nullptr;
         node = node->next) {
        if (node->value.name.equals(name)) {
            return node->value.function;
        }
    }

    Arena* scratch = GetScratchLinux();
    u64 scratch_mark = scratch->mark();
    DEFER {
        scratch->restore(scratch_mark);
    };

    const char* symbol_name = name.toCstr(scratch);
    const char* filename_cstr =
        library->filename.str != nullptr ? library->filename.toCstr(scratch) : "<unknown>";
    dlerror();
    void* symbol = dlsym(library->internal_data, symbol_name);
    const char* error = dlerror();

    if (error != nullptr || !symbol) {
        LOG_DEBUG(
            "Failed to load symbol '%s' from '%s': %s",
            symbol_name,
            filename_cstr,
            error != nullptr ? error : "symbol not found"
        );
        return nullptr;
    }

    DynamicLibraryFunction function = {};
    function.name = String::copy(library->arena, name);
    function.function = symbol;
    library->functions.push(function);
    return symbol;
}

String GetDynamicLibraryExtension(void) {
    return String::lit(".so");
}

String GetDynamicLibraryPrefix(void) {
    return String::lit("lib");
}

ErrorCode CopyFile(
    String source,
    String dest,
    bool overwrite_if_exists
) {
    Arena* scratch = GetScratchLinux();
    u64 scratch_mark = scratch->mark();
    DEFER {
        scratch->restore(scratch_mark);
    };

    const char* source_path = source.toCstr(scratch);
    const char* dest_path = dest.toCstr(scratch);

    int source_fd = -1;
    DEFER {
        if (source_fd >= 0) {
            close(source_fd);
        }
    };

    int dest_fd = -1;
    DEFER {
        if (dest_fd >= 0) {
            close(dest_fd);
        }
    };

    source_fd = open(source_path, O_RDONLY);
    if (source_fd < 0) {
        return GetCopyErrorLinux(errno);
    }

    struct stat source_stat = {};
    if (fstat(source_fd, &source_stat) != 0) {
        return GetCopyErrorLinux(errno);
    }

    int dest_flags = O_WRONLY | O_CREAT;
    dest_flags |= overwrite_if_exists ? O_TRUNC : O_EXCL;

    dest_fd = open(dest_path, dest_flags, source_stat.st_mode);
    if (dest_fd < 0) {
        return GetCopyErrorLinux(errno);
    }

    ErrorCode result = ErrorCode::SUCCESS;
    char buffer[64 * KB];
    for (;;) {
        ssize_t bytes_read = read(source_fd, buffer, sizeof(buffer));
        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            result = GetCopyErrorLinux(errno);
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
                result = GetCopyErrorLinux(errno);
                break;
            }
            total_written += bytes_written;
        }

        if (result != ErrorCode::SUCCESS) {
            break;
        }
    }

    return result;
}

ArrayList<const char*> GetVulkanInstanceExtensions(Arena* arena) {
    ASSUME(arena != nullptr);

    ArrayList<const char*> extensions = ArrayList<const char*>::make(arena);
    extensions.push(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    return extensions;
}

bool CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* out_surface) {
    if (instance == VK_NULL_HANDLE || out_surface == nullptr ||
        linux_state.connection == nullptr ||
        linux_state.window == XCB_WINDOW_NONE) {
        return false;
    }

    *out_surface = VK_NULL_HANDLE;

    VkXcbSurfaceCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    create_info.connection = linux_state.connection;
    create_info.window = linux_state.window;

    return vkCreateXcbSurfaceKHR(instance, &create_info, nullptr, out_surface) ==
           VK_SUCCESS;
}

}

#endif
