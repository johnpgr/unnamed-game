#include "platform/win32/internal.h"

ArrayList<const char*> pvkGetInstanceExtensions(Arena* arena) {
    ASSUME(arena != nullptr);

    ArrayList<const char*> extensions = ArrayList<const char*>::make(arena);
    extensions.push(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    return extensions;
}

bool pvkCreateSurface(
    VkInstance instance,
    VkSurfaceKHR* out_surface
) {
    if (instance == VK_NULL_HANDLE || out_surface == nullptr ||
        win32_state.instance == nullptr || win32_state.window == nullptr) {
        return false;
    }

    *out_surface = VK_NULL_HANDLE;

    VkWin32SurfaceCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = win32_state.instance;
    create_info.hwnd = win32_state.window;

    VkResult result =
        vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, out_surface);
    return result == VK_SUCCESS;
}
