#include <vulkan/vulkan_xcb.h>

#include "platform/linux/internal.h"

ArrayList<const char*> pvkGetInstanceExtensions(Arena* arena) {
    ASSUME(arena != nullptr);

    ArrayList<const char*> extensions = ArrayList<const char*>::make(arena);
    extensions.push(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    return extensions;
}

bool pvkCreateSurface(
    VkInstance instance,
    VkSurfaceKHR* out_surface
) {
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
