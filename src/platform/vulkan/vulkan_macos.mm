#include "platform/macos/internal.h"

ArrayList<const char*> pvkGetInstanceExtensions(Arena* arena) {
    ASSUME(arena != nullptr);

    ArrayList<const char*> extensions = ArrayList<const char*>::make(arena);
    extensions.push(VK_KHR_SURFACE_EXTENSION_NAME);

    if (pvkHasInstanceExtensionMacOS(VK_EXT_METAL_SURFACE_EXTENSION_NAME)) {
        extensions.push(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    }

    if (pvkHasInstanceExtensionMacOS(
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        )) {
        extensions.push(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }

    return extensions;
}

bool pvkCreateSurface(
    VkInstance instance,
    VkSurfaceKHR* out_surface
) {
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
