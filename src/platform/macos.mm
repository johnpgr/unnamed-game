#if OS_MAC

#pragma push_macro("internal")
#undef internal
#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>
#include <QuartzCore/CAMetalLayer.h>
#include <copyfile.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#include <vulkan/vulkan_metal.h>
#pragma pop_macro("internal")

namespace Platform {

struct MacPlatformState {
    NSApplication* application;
    NSWindow* window;
    NSObject<NSWindowDelegate>* delegate;
    int width;
    int height;
    bool initialized;
    bool should_close;
    bool visible;
    bool resizable;
};

extern MacPlatformState mac_state;

void Fail(const char* message) {
    int error = errno;
    if (error != 0) {
        LOG_FATAL("%s failed: %s", message, strerror(error));
    } else {
        LOG_FATAL("%s", message);
    }
    abort();
}

}

@interface CppGamingWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation CppGamingWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    Platform::mac_state.should_close = true;
    Platform::mac_state.visible = false;
}

- (void)windowDidResize:(NSNotification*)notification {
    NSWindow* window = [notification object];
    NSRect content_rect = [window contentRectForFrameRect:[window frame]];
    Platform::mac_state.width = (int)content_rect.size.width;
    Platform::mac_state.height = (int)content_rect.size.height;
}

@end

namespace Platform {

MacPlatformState mac_state = {};
internal Arena mac_scratch_arena = {};
internal bool mac_scratch_initialized = false;

internal Arena* GetScratchMacOS(void);

internal bool HasInstanceExtensionMacOS(const char* extension_name) {
    if (extension_name == nullptr || extension_name[0] == 0) {
        return false;
    }

    u32 extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        nullptr
    );
    if (result != VK_SUCCESS || extension_count == 0) {
        return false;
    }

    Arena* scratch = GetScratchMacOS();
    u64 scratch_mark = scratch->mark();
    DEFER {
        scratch->restore(scratch_mark);
    };

    VkExtensionProperties* extensions =
        scratch->push<VkExtensionProperties>((u64)extension_count);
    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        extensions
    );
    if (result != VK_SUCCESS) {
        return false;
    }

    for (u32 i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, extension_name) == 0) {
            return true;
        }
    }

    return false;
}

internal NSString* ToNSStringMacOS(String text) {
    return [[[NSString alloc] initWithBytes:text.str
                                     length:(NSUInteger)text.size
                                   encoding:NSUTF8StringEncoding] autorelease];
}

internal Arena* GetScratchMacOS(void) {
    if (!mac_scratch_initialized) {
        mac_scratch_arena = CreateArena();
        mac_scratch_initialized = true;
    }
    return &mac_scratch_arena;
}

internal bool GetExecutableDirMacOS(char* buffer, usize buffer_size) {
    ASSERT(buffer != nullptr, "Buffer must not be null!");
    ASSERT(buffer_size > 0, "Buffer size must be non-zero!");

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size > buffer_size) {
        return false;
    }

    if (_NSGetExecutablePath(buffer, &size) != 0) {
        return false;
    }

    char* slash = strrchr(buffer, '/');
    if (!slash) {
        return false;
    }
    *slash = 0;
    return true;
}

internal bool TryLoadDynamicLibraryMacOS(const char* path, void** out_handle) {
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

internal ErrorCode GetCopyErrorMacOS(int error) {
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

internal NSUInteger GetWindowStyleMacOS(void) {
    NSUInteger style = NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable;
    if (mac_state.resizable) {
        style |= NSWindowStyleMaskResizable;
    }
    return style;
}

void CreateWindow(String title, int width, int height) {
    @autoreleasepool {
        if (mac_state.initialized) {
            DestroyWindow();
        }

        mac_state.application = [NSApplication sharedApplication];
        [mac_state.application
            setActivationPolicy:NSApplicationActivationPolicyRegular];
        [mac_state.application finishLaunching];

        mac_state.resizable = true;
        NSRect rect = NSMakeRect(0, 0, width, height);
        mac_state.window = [[NSWindow alloc]
            initWithContentRect:rect
                      styleMask:GetWindowStyleMacOS()
                        backing:NSBackingStoreBuffered
                          defer:NO];
        if (mac_state.window == nil) {
            Fail("Failed to create macOS window.");
        }

        mac_state.delegate = [[CppGamingWindowDelegate alloc] init];
        [mac_state.window setDelegate:mac_state.delegate];
        [mac_state.window setTitle:ToNSStringMacOS(
            title.size > 0 ? title : String::lit("cpp-gaming")
        )];

        mac_state.width = width;
        mac_state.height = height;
        mac_state.initialized = true;
        mac_state.should_close = false;
        mac_state.visible = false;
    }
}

void DestroyWindow(void) {
    @autoreleasepool {
        if (!mac_state.initialized) {
            return;
        }

        if (mac_state.window != nil) {
            [mac_state.window setDelegate:nil];
            [mac_state.window close];
            mac_state.window = nil;
        }
        if (mac_state.delegate != nil) {
            [mac_state.delegate release];
            mac_state.delegate = nil;
        }

        mac_state = {};
    }
}

bool ShouldWindowClose(void) {
    return mac_state.should_close;
}

void PollEvents(void) {
    @autoreleasepool {
        if (!mac_state.initialized || mac_state.application == nil) {
            return;
        }

        for (;;) {
            NSEvent* event = [mac_state.application
                nextEventMatchingMask:NSEventMaskAny
                            untilDate:[NSDate distantPast]
                               inMode:NSDefaultRunLoopMode
                              dequeue:YES];
            if (event == nil) {
                break;
            }

            [mac_state.application sendEvent:event];
        }

        [mac_state.application updateWindows];
    }
}

void SetWindowTitle(String title) {
    @autoreleasepool {
        if (mac_state.window != nil) {
            [mac_state.window setTitle:ToNSStringMacOS(title)];
        }
    }
}

void GetWindowSize(int* width, int* height) {
    if (width) {
        *width = mac_state.width;
    }
    if (height) {
        *height = mac_state.height;
    }
}

void SetWindowSize(int width, int height) {
    @autoreleasepool {
        if (mac_state.window == nil) {
            return;
        }

        NSRect frame = [mac_state.window frameRectForContentRect:
            NSMakeRect(0, 0, width, height)];
        [mac_state.window setFrame:frame display:YES];
        mac_state.width = width;
        mac_state.height = height;
    }
}

void SetWindowResizable(bool resizable) {
    @autoreleasepool {
        mac_state.resizable = resizable;
        if (mac_state.window == nil) {
            return;
        }

        NSUInteger style = [mac_state.window styleMask];
        if (resizable) {
            style |= NSWindowStyleMaskResizable;
        } else {
            style &= ~NSWindowStyleMaskResizable;
        }
        [mac_state.window setStyleMask:style];
    }
}

void PresentWindow(void) {
    @autoreleasepool {
        if (mac_state.window != nil) {
            [mac_state.window displayIfNeeded];
        }
    }
}

void ShowWindow(void) {
    @autoreleasepool {
        if (!mac_state.initialized || mac_state.visible ||
            mac_state.window == nil) {
            return;
        }

        [mac_state.window makeKeyAndOrderFront:nil];
        [mac_state.application activateIgnoringOtherApps:YES];
        mac_state.visible = true;
    }
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

bool LoadDynamicLibrary(
    Arena* arena,
    String name,
    DynamicLibrary* out_library
) {
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

    Arena* scratch = GetScratchMacOS();
    u64 scratch_mark = scratch->mark();
    DEFER {
        scratch->restore(scratch_mark);
    };

    const char* filename_cstr = filename.toCstr(scratch);
    char executable_dir[PATH_MAX] = {};
    bool have_executable_dir =
        GetExecutableDirMacOS(executable_dir, sizeof(executable_dir));
    char cwd[PATH_MAX] = {};
    bool have_cwd = getcwd(cwd, sizeof(cwd)) != nullptr;
    void* handle = nullptr;

    if (have_executable_dir) {
        String resolved_path =
            String::fmt(scratch, "%s/%s", executable_dir, filename_cstr);
        if (TryLoadDynamicLibraryMacOS((const char*)resolved_path.str, &handle)) {
            out_library->filename = String::copy(arena, resolved_path);
        }
    }

    if (!handle && have_cwd) {
        String resolved_path = String::fmt(scratch, "%s/%s", cwd, filename_cstr);
        if (TryLoadDynamicLibraryMacOS((const char*)resolved_path.str, &handle)) {
            out_library->filename = String::copy(arena, resolved_path);
        }
    }

    if (!handle && TryLoadDynamicLibraryMacOS(filename_cstr, &handle)) {
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

    Arena* scratch = GetScratchMacOS();
    u64 scratch_mark = scratch->mark();
    DEFER {
        scratch->restore(scratch_mark);
    };

    const char* symbol_name = name.toCstr(scratch);
    dlerror();
    void* symbol = dlsym(library->internal_data, symbol_name);
    const char* error = dlerror();
    if (error != nullptr || !symbol) {
        return nullptr;
    }

    DynamicLibraryFunction function = {};
    function.name = String::copy(library->arena, name);
    function.function = symbol;
    library->functions.push(function);
    return symbol;
}

String GetDynamicLibraryExtension(void) {
    return String::lit(".dylib");
}

String GetDynamicLibraryPrefix(void) {
    return String::lit("lib");
}

ErrorCode CopyFile(
    String source,
    String dest,
    bool overwrite_if_exists
) {
    Arena* scratch = GetScratchMacOS();
    u64 scratch_mark = scratch->mark();
    DEFER {
        scratch->restore(scratch_mark);
    };

    const char* source_path = source.toCstr(scratch);
    const char* dest_path = dest.toCstr(scratch);
    copyfile_flags_t flags = COPYFILE_ALL;
    if (!overwrite_if_exists) {
        flags |= COPYFILE_EXCL;
    }

    int copied = copyfile(source_path, dest_path, nullptr, flags);
    int error = copied == 0 ? 0 : errno;
    return GetCopyErrorMacOS(error);
}

ArrayList<const char*> GetVulkanInstanceExtensions(Arena* arena) {
    ASSUME(arena != nullptr);

    ArrayList<const char*> extensions = ArrayList<const char*>::make(arena);
    extensions.push(VK_KHR_SURFACE_EXTENSION_NAME);

    if (HasInstanceExtensionMacOS(VK_EXT_METAL_SURFACE_EXTENSION_NAME)) {
        extensions.push(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    }

    if (HasInstanceExtensionMacOS(
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        )) {
        extensions.push(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }

    return extensions;
}

bool CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* out_surface) {
    if (instance == VK_NULL_HANDLE || out_surface == nullptr ||
        mac_state.window == nil) {
        return false;
    }

    NSView* content_view = [mac_state.window contentView];
    if (content_view == nil) {
        return false;
    }

    CAMetalLayer* layer = nil;
    if ([[content_view layer] isKindOfClass:[CAMetalLayer class]]) {
        layer = (CAMetalLayer*)[content_view layer];
    } else {
        layer = [CAMetalLayer layer];
        [content_view setWantsLayer:YES];
        [content_view setLayer:layer];
    }

    if (layer == nil) {
        return false;
    }

    CGFloat scale = [mac_state.window backingScaleFactor];
    layer.contentsScale = scale;
    layer.frame = [content_view bounds];
    NSSize view_size = [content_view bounds].size;
    layer.drawableSize =
        CGSizeMake(view_size.width * scale, view_size.height * scale);

    PFN_vkCreateMetalSurfaceEXT create_metal_surface =
        (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(
            instance,
            "vkCreateMetalSurfaceEXT"
        );
    if (create_metal_surface == nullptr) {
        return false;
    }

    *out_surface = VK_NULL_HANDLE;

    VkMetalSurfaceCreateInfoEXT create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    create_info.pLayer = layer;

    return create_metal_surface(instance, &create_info, nullptr, out_surface) ==
           VK_SUCCESS;
}

}

#endif
