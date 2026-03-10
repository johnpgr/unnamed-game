#pragma once

#pragma push_macro("internal")
#pragma push_macro("assert")
#undef internal
#undef assert
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
#pragma pop_macro("assert")
#pragma pop_macro("internal")

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

internal MacPlatformState mac_state = {};
internal Arena mac_scratch_arena = {};
internal bool mac_scratch_initialized = false;

internal bool pvkHasInstanceExtensionMacOS(const char* extension_name) {
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

    VkExtensionProperties* extensions = (VkExtensionProperties*)malloc(
        (size_t)extension_count * sizeof(VkExtensionProperties)
    );
    if (extensions == nullptr) {
        return false;
    }

    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        extensions
    );
    if (result != VK_SUCCESS) {
        free(extensions);
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    free(extensions);
    return found;
}

@interface CppGamingWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation CppGamingWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    mac_state.should_close = true;
    mac_state.visible = false;
}

- (void)windowDidResize:(NSNotification*)notification {
    NSWindow* window = [notification object];
    NSRect content_rect = [window contentRectForFrameRect:[window frame]];
    mac_state.width = (int)content_rect.size.width;
    mac_state.height = (int)content_rect.size.height;
}

@end

internal void pFail(const char* message) {
    LOG_FATAL("%s", message);
    abort();
}

internal NSString* pwToNSStringMacOS(String text) {
    return [[[NSString alloc] initWithBytes:text.str
                                     length:(NSUInteger)text.size
                                   encoding:NSUTF8StringEncoding] autorelease];
}

internal Arena* pGetScratchMacOS(void) {
    if (!mac_scratch_initialized) {
        mac_scratch_arena = Arena::make();
        mac_scratch_initialized = true;
    }
    return &mac_scratch_arena;
}

internal bool pGetExecutableDirMacOS(char* buffer, usize buffer_size) {
    assert(buffer != nullptr, "Buffer must not be null!");
    assert(buffer_size > 0, "Buffer size must be non-zero!");

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

internal bool pdlTryLoadLibraryMacOS(const char* path, void** out_handle) {
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

internal PlatformErrorCode pfsGetCopyErrorMacOS(int error) {
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

internal NSUInteger pwGetWindowStyleMacOS(void) {
    NSUInteger style = NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable;
    if (mac_state.resizable) {
        style |= NSWindowStyleMaskResizable;
    }
    return style;
}
