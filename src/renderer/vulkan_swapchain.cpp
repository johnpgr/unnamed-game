#include "renderer/vulkan.h"

#include "base/arena.h"
#include "base/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct SwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    u32 format_count;
    VkPresentModeKHR *present_modes;
    u32 present_mode_count;
};

internal b32
query_swapchain_support(
    Arena *arena,
    VkPhysicalDevice physical_device,
    SwapchainSupportInfo *out_info
) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);
    assume(out_info != nullptr);

    *out_info = {};

    if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
           physical_device,
           vk_state.surface,
           &out_info->capabilities
       ) != VK_SUCCESS) {
        return false;
    }

    if(vkGetPhysicalDeviceSurfaceFormatsKHR(
           physical_device,
           vk_state.surface,
           &out_info->format_count,
           nullptr
       ) != VK_SUCCESS ||
       out_info->format_count == 0) {
        return false;
    }

    out_info->formats =
        push_array(arena, out_info->format_count, VkSurfaceFormatKHR);
    if(vkGetPhysicalDeviceSurfaceFormatsKHR(
           physical_device,
           vk_state.surface,
           &out_info->format_count,
           out_info->formats
       ) != VK_SUCCESS) {
        return false;
    }

    if(vkGetPhysicalDeviceSurfacePresentModesKHR(
           physical_device,
           vk_state.surface,
           &out_info->present_mode_count,
           nullptr
       ) != VK_SUCCESS ||
       out_info->present_mode_count == 0) {
        return false;
    }

    out_info->present_modes =
        push_array(arena, out_info->present_mode_count, VkPresentModeKHR);
    if(vkGetPhysicalDeviceSurfacePresentModesKHR(
           physical_device,
           vk_state.surface,
           &out_info->present_mode_count,
           out_info->present_modes
       ) != VK_SUCCESS) {
        return false;
    }

    return true;
}

internal VkSurfaceFormatKHR
choose_surface_format(SwapchainSupportInfo *support) {
    assume(support != nullptr);

    for(u32 index = 0; index < support->format_count; ++index) {
        VkSurfaceFormatKHR format = support->formats[index];
        if(format.format == VK_FORMAT_B8G8R8A8_UNORM &&
           format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return support->formats[0];
}

internal VkPresentModeKHR
choose_present_mode(SwapchainSupportInfo *support) {
    assume(support != nullptr);

    for(u32 index = 0; index < support->present_mode_count; ++index) {
        if(support->present_modes[index] == VK_PRESENT_MODE_FIFO_KHR) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    return support->present_modes[0];
}

internal VkExtent2D
choose_swapchain_extent(SwapchainSupportInfo *support) {
    assume(support != nullptr);

    if(support->capabilities.currentExtent.width != UINT32_MAX) {
        return support->capabilities.currentExtent;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(
        vk_state.window,
        &framebuffer_width,
        &framebuffer_height
    );

    VkExtent2D extent = {};
    extent.width = (u32)framebuffer_width;
    extent.height = (u32)framebuffer_height;

    if(extent.width < support->capabilities.minImageExtent.width) {
        extent.width = support->capabilities.minImageExtent.width;
    }
    if(extent.width > support->capabilities.maxImageExtent.width) {
        extent.width = support->capabilities.maxImageExtent.width;
    }
    if(extent.height < support->capabilities.minImageExtent.height) {
        extent.height = support->capabilities.minImageExtent.height;
    }
    if(extent.height > support->capabilities.maxImageExtent.height) {
        extent.height = support->capabilities.maxImageExtent.height;
    }

    return extent;
}

internal b32
wait_for_nonzero_framebuffer(void) {
    int framebuffer_width = 0;
    int framebuffer_height = 0;

    while(framebuffer_width == 0 || framebuffer_height == 0) {
        if(glfwWindowShouldClose(vk_state.window)) {
            return false;
        }

        glfwGetFramebufferSize(
            vk_state.window,
            &framebuffer_width,
            &framebuffer_height
        );
        if(framebuffer_width == 0 || framebuffer_height == 0) {
            glfwWaitEvents();
        }
    }

    return true;
}

internal b32
create_swapchain(void) {
    TemporaryMemory temporary_memory = begin_temporary_memory(vk_state.arena);
    SwapchainSupportInfo support = {};
    if(!query_swapchain_support(
           vk_state.arena,
           vk_state.physical_device,
           &support
       )) {
        end_temporary_memory(temporary_memory);
        return false;
    }

    VkSurfaceFormatKHR surface_format = choose_surface_format(&support);
    VkPresentModeKHR present_mode = choose_present_mode(&support);
    VkExtent2D extent = choose_swapchain_extent(&support);

    u32 image_count = support.capabilities.minImageCount + 1;
    if(support.capabilities.maxImageCount > 0 &&
       image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }
    if(image_count > MAX_SWAPCHAIN_IMAGES) {
        LOG_FATAL(
            "Swapchain image count %u exceeds MAX_SWAPCHAIN_IMAGES.",
            image_count
        );
        end_temporary_memory(temporary_memory);
        return false;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = vk_state.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    if(vkCreateSwapchainKHR(
           vk_state.device,
           &create_info,
           nullptr,
           &vk_state.swapchain
       ) != VK_SUCCESS) {
        end_temporary_memory(temporary_memory);
        return false;
    }

    if(vkGetSwapchainImagesKHR(
           vk_state.device,
           vk_state.swapchain,
           &image_count,
           vk_state.swapchain_images
       ) != VK_SUCCESS) {
        end_temporary_memory(temporary_memory);
        return false;
    }

    vk_state.swapchain_format = surface_format.format;
    vk_state.swapchain_extent = extent;
    vk_state.swapchain_image_count = image_count;

    for(u32 image_index = 0; image_index < image_count; ++image_index) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = vk_state.swapchain_images[image_index];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = vk_state.swapchain_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;

        if(vkCreateImageView(
               vk_state.device,
               &view_info,
               nullptr,
               &vk_state.swapchain_views[image_index]
           ) != VK_SUCCESS) {
            end_temporary_memory(temporary_memory);
            return false;
        }
    }

    end_temporary_memory(temporary_memory);
    return true;
}

internal void
cleanup_swapchain(void) {
    for(u32 image_index = 0;
        image_index < ARRAY_COUNT(vk_state.swapchain_views);
        ++image_index) {
        if(vk_state.swapchain_views[image_index] != VK_NULL_HANDLE) {
            vkDestroyImageView(
                vk_state.device,
                vk_state.swapchain_views[image_index],
                nullptr
            );
            vk_state.swapchain_views[image_index] = VK_NULL_HANDLE;
        }
        vk_state.swapchain_images[image_index] = VK_NULL_HANDLE;
    }

    if(vk_state.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vk_state.device, vk_state.swapchain, nullptr);
        vk_state.swapchain = VK_NULL_HANDLE;
    }

    vk_state.swapchain_format = VK_FORMAT_UNDEFINED;
    vk_state.swapchain_extent = {};
    vk_state.swapchain_image_count = 0;
}

internal b32
recreate_swapchain(void) {
    if(!wait_for_nonzero_framebuffer()) {
        return false;
    }

    if(vkDeviceWaitIdle(vk_state.device) != VK_SUCCESS) {
        return false;
    }

    cleanup_pipeline();
    cleanup_swapchain();

    if(!create_swapchain()) {
        return false;
    }
    if(!create_pipeline()) {
        return false;
    }

    return true;
}
