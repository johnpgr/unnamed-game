#include "renderer/vulkan.h"

#include "base/arena.h"
#include "base/log.h"

#include <cstring>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifndef NDEBUG
global_variable char const *validation_layer_name =
    "VK_LAYER_KHRONOS_validation";
#endif
global_variable char const *portability_subset_extension_name =
    "VK_KHR_portability_subset";

internal char const *
get_glfw_error_string(void) {
    char const *description = nullptr;
    glfwGetError(&description);
    return description != nullptr ? description : "Unknown GLFW error";
}

internal u32
get_target_api_version(void) {
    u32 api_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion enumerate_instance_version =
        (PFN_vkEnumerateInstanceVersion)
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");

    if(enumerate_instance_version != nullptr &&
       enumerate_instance_version(&api_version) == VK_SUCCESS) {
        return api_version;
    }

    return VK_API_VERSION_1_0;
}

internal b32
has_instance_extension(Arena *arena, char const *extension_name) {
    assume(arena != nullptr);
    assume(extension_name != nullptr);

    u32 extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        nullptr
    );
    if(result != VK_SUCCESS || extension_count == 0) {
        return false;
    }

    TemporaryMemory temporary_memory = begin_temporary_memory(arena);
    VkExtensionProperties *extensions =
        push_array(arena, extension_count, VkExtensionProperties);

    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        extensions
    );
    if(result != VK_SUCCESS) {
        end_temporary_memory(temporary_memory);
        return false;
    }

    b32 found = false;
    for(u32 index = 0; index < extension_count; ++index) {
        if(strcmp(extensions[index].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    end_temporary_memory(temporary_memory);
    return found;
}

internal char const **
get_instance_extensions(Arena *arena, u32 *out_extension_count) {
    assume(arena != nullptr);
    assume(out_extension_count != nullptr);

    u32 glfw_extension_count = 0;
    char const **glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    if(glfw_extensions == nullptr || glfw_extension_count == 0) {
        *out_extension_count = 0;
        return nullptr;
    }

    u32 extra_extension_count = 0;
    if(has_instance_extension(
           arena,
           VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
       )) {
        ++extra_extension_count;
    }

#ifndef NDEBUG
    if(has_instance_extension(arena, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        ++extra_extension_count;
    }
#endif

    u32 extension_count = glfw_extension_count + extra_extension_count;
    char const **extensions = push_array(arena, extension_count, char const *);

    u32 extension_index = 0;
    for(u32 glfw_index = 0; glfw_index < glfw_extension_count; ++glfw_index) {
        extensions[extension_index++] = glfw_extensions[glfw_index];
    }

    if(has_instance_extension(
           arena,
           VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
       )) {
        extensions[extension_index++] =
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    }

#ifndef NDEBUG
    if(has_instance_extension(arena, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        extensions[extension_index++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }
#endif

    *out_extension_count = extension_index;
    return extensions;
}

internal b32
has_layer(Arena *arena, char const *layer_name) {
    assume(arena != nullptr);
    assume(layer_name != nullptr);

    u32 layer_count = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    if(result != VK_SUCCESS || layer_count == 0) {
        return false;
    }

    TemporaryMemory temporary_memory = begin_temporary_memory(arena);
    VkLayerProperties *layers =
        push_array(arena, layer_count, VkLayerProperties);

    result = vkEnumerateInstanceLayerProperties(&layer_count, layers);
    if(result != VK_SUCCESS) {
        end_temporary_memory(temporary_memory);
        return false;
    }

    b32 found = false;
    for(u32 index = 0; index < layer_count; ++index) {
        if(strcmp(layers[index].layerName, layer_name) == 0) {
            found = true;
            break;
        }
    }

    end_temporary_memory(temporary_memory);
    return found;
}

#ifndef NDEBUG
internal VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    VkDebugUtilsMessengerCallbackDataEXT const *callback_data,
    void *user_data
) {
    (void)message_types;
    (void)user_data;

    char const *message =
        (callback_data != nullptr && callback_data->pMessage != nullptr)
            ? callback_data->pMessage
            : "Unknown Vulkan validation message";

    if((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) !=
       0) {
        LOG_ERROR("Vulkan: %s", message);
    } else if(
        (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) !=
        0
    ) {
        LOG_WARN("Vulkan: %s", message);
    } else {
        LOG_INFO("Vulkan: %s", message);
    }

    return VK_FALSE;
}

internal void
build_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *out_create_info
) {
    assume(out_create_info != nullptr);

    *out_create_info = {};
    out_create_info->sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    out_create_info->messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    out_create_info->messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    out_create_info->pfnUserCallback = debug_callback;
}

internal b32
create_debug_messenger(void) {
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            vk_state.instance,
            "vkCreateDebugUtilsMessengerEXT"
        );
    if(create_debug_utils_messenger == nullptr) {
        return false;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    build_debug_messenger_create_info(&create_info);

    return create_debug_utils_messenger(
               vk_state.instance,
               &create_info,
               nullptr,
               &vk_state.debug_messenger
           ) == VK_SUCCESS;
}
#endif

internal b32
has_device_extension(
    Arena *arena,
    VkPhysicalDevice physical_device,
    char const *extension_name
) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);
    assume(extension_name != nullptr);

    u32 extension_count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(
        physical_device,
        nullptr,
        &extension_count,
        nullptr
    );
    if(result != VK_SUCCESS || extension_count == 0) {
        return false;
    }

    TemporaryMemory temporary_memory = begin_temporary_memory(arena);
    VkExtensionProperties *extensions =
        push_array(arena, extension_count, VkExtensionProperties);

    result = vkEnumerateDeviceExtensionProperties(
        physical_device,
        nullptr,
        &extension_count,
        extensions
    );
    if(result != VK_SUCCESS) {
        end_temporary_memory(temporary_memory);
        return false;
    }

    b32 found = false;
    for(u32 index = 0; index < extension_count; ++index) {
        if(strcmp(extensions[index].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    end_temporary_memory(temporary_memory);
    return found;
}

internal b32
supports_dynamic_rendering(Arena *arena, VkPhysicalDevice physical_device) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    b32 has_dynamic_rendering_extension = has_device_extension(
        arena,
        physical_device,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    );

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {};
    dynamic_rendering_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    if(properties.apiVersion >= VK_API_VERSION_1_3) {
        features2.pNext = &features13;
    } else if(has_dynamic_rendering_extension) {
        features2.pNext = &dynamic_rendering_features;
    }

    vkGetPhysicalDeviceFeatures2(physical_device, &features2);

    if(properties.apiVersion >= VK_API_VERSION_1_3) {
        return features13.dynamicRendering == VK_TRUE;
    }

    return has_dynamic_rendering_extension &&
           dynamic_rendering_features.dynamicRendering == VK_TRUE;
}

internal b32
find_graphics_queue_family(
    Arena *arena,
    VkPhysicalDevice physical_device,
    u32 *out_queue_family_index
) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);
    assume(out_queue_family_index != nullptr);

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        nullptr
    );
    if(queue_family_count == 0) {
        return false;
    }

    TemporaryMemory temporary_memory = begin_temporary_memory(arena);
    VkQueueFamilyProperties *queue_families =
        push_array(arena, queue_family_count, VkQueueFamilyProperties);

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        queue_families
    );

    b32 found = false;
    for(u32 index = 0; index < queue_family_count; ++index) {
        if((queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
           queue_families[index].queueCount == 0) {
            continue;
        }

        VkBool32 supports_present = VK_FALSE;
        if(vkGetPhysicalDeviceSurfaceSupportKHR(
               physical_device,
               index,
               vk_state.surface,
               &supports_present
           ) != VK_SUCCESS ||
           supports_present == VK_FALSE) {
            continue;
        }

        *out_queue_family_index = index;
        found = true;
        break;
    }

    end_temporary_memory(temporary_memory);
    return found;
}

internal u32
score_device(Arena *arena, VkPhysicalDevice physical_device) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);

    u32 graphics_queue_family_index = 0;
    if(!find_graphics_queue_family(
           arena,
           physical_device,
           &graphics_queue_family_index
       )) {
        return 0;
    }

    if(!has_device_extension(
           arena,
           physical_device,
           VK_KHR_SWAPCHAIN_EXTENSION_NAME
       )) {
        return 0;
    }

    if(!supports_dynamic_rendering(arena, physical_device)) {
        return 0;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    u32 score = properties.limits.maxImageDimension2D;
    if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    return score;
}

internal b32
create_device(Arena *arena) {
    assume(arena != nullptr);
    assume(vk_state.physical_device != VK_NULL_HANDLE);

    if(!find_graphics_queue_family(
           arena,
           vk_state.physical_device,
           &vk_state.graphics_queue_family_index
       )) {
        return false;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(vk_state.physical_device, &properties);

    vk_state.dynamic_rendering_supported =
        supports_dynamic_rendering(arena, vk_state.physical_device);
    if(!vk_state.dynamic_rendering_supported) {
        LOG_FATAL("Dynamic rendering not supported on selected Vulkan device.");
        return false;
    }

    f32 graphics_queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = vk_state.graphics_queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &graphics_queue_priority;

    b32 needs_dynamic_rendering_extension =
        properties.apiVersion < VK_API_VERSION_1_3;
    b32 needs_portability_subset_extension = has_device_extension(
        arena,
        vk_state.physical_device,
        portability_subset_extension_name
    );

    char const *device_extensions[3] = {};
    u32 device_extension_count = 0;
    device_extensions[device_extension_count++] =
        VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if(needs_dynamic_rendering_extension) {
        device_extensions[device_extension_count++] =
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
    }
    if(needs_portability_subset_extension) {
        device_extensions[device_extension_count++] =
            portability_subset_extension_name;
    }

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {};
    dynamic_rendering_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_create_info;
    create_info.enabledExtensionCount = device_extension_count;
    create_info.ppEnabledExtensionNames = device_extensions;
    if(properties.apiVersion >= VK_API_VERSION_1_3) {
        create_info.pNext = &features13;
    } else {
        create_info.pNext = &dynamic_rendering_features;
    }

    if(vkCreateDevice(
           vk_state.physical_device,
           &create_info,
           nullptr,
           &vk_state.device
       ) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(
        vk_state.device,
        vk_state.graphics_queue_family_index,
        0,
        &vk_state.graphics_queue
    );

    return vk_state.device != VK_NULL_HANDLE &&
           vk_state.graphics_queue != VK_NULL_HANDLE;
}

internal b32
pick_physical_device(Arena *arena) {
    assume(arena != nullptr);

    u32 physical_device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(
        vk_state.instance,
        &physical_device_count,
        nullptr
    );
    if(result != VK_SUCCESS || physical_device_count == 0) {
        return false;
    }

    TemporaryMemory temporary_memory = begin_temporary_memory(arena);
    VkPhysicalDevice *physical_devices =
        push_array(arena, physical_device_count, VkPhysicalDevice);

    result = vkEnumeratePhysicalDevices(
        vk_state.instance,
        &physical_device_count,
        physical_devices
    );
    if(result != VK_SUCCESS) {
        end_temporary_memory(temporary_memory);
        return false;
    }

    vk_state.physical_device = VK_NULL_HANDLE;
    u32 best_score = 0;
    for(u32 index = 0; index < physical_device_count; ++index) {
        u32 score = score_device(arena, physical_devices[index]);
        if(score > best_score) {
            best_score = score;
            vk_state.physical_device = physical_devices[index];
        }
    }

    end_temporary_memory(temporary_memory);
    return vk_state.physical_device != VK_NULL_HANDLE;
}

internal b32
load_dynamic_rendering_functions(void) {
    vk_state.cmd_begin_rendering = (PFN_vkCmdBeginRenderingKHR)
        vkGetDeviceProcAddr(vk_state.device, "vkCmdBeginRenderingKHR");
    if(vk_state.cmd_begin_rendering == nullptr) {
        vk_state.cmd_begin_rendering = (PFN_vkCmdBeginRenderingKHR)
            vkGetDeviceProcAddr(vk_state.device, "vkCmdBeginRendering");
    }

    vk_state.cmd_end_rendering = (PFN_vkCmdEndRenderingKHR)
        vkGetDeviceProcAddr(vk_state.device, "vkCmdEndRenderingKHR");
    if(vk_state.cmd_end_rendering == nullptr) {
        vk_state.cmd_end_rendering = (PFN_vkCmdEndRenderingKHR)
            vkGetDeviceProcAddr(vk_state.device, "vkCmdEndRendering");
    }

    return vk_state.cmd_begin_rendering != nullptr &&
           vk_state.cmd_end_rendering != nullptr;
}

internal b32
create_frame_sync_objects(void) {
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if(vkCreateSemaphore(
           vk_state.device,
           &semaphore_info,
           nullptr,
           &vk_state.image_available_semaphore
       ) != VK_SUCCESS) {
        return false;
    }

    if(vkCreateSemaphore(
           vk_state.device,
           &semaphore_info,
           nullptr,
           &vk_state.render_finished_semaphore
       ) != VK_SUCCESS) {
        return false;
    }

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    return vkCreateFence(
               vk_state.device,
               &fence_info,
               nullptr,
               &vk_state.frame_fence
           ) == VK_SUCCESS;
}

internal void
cleanup_frame_sync_objects(void) {
    if(vk_state.frame_fence != VK_NULL_HANDLE) {
        vkDestroyFence(vk_state.device, vk_state.frame_fence, nullptr);
        vk_state.frame_fence = VK_NULL_HANDLE;
    }
    if(vk_state.render_finished_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(
            vk_state.device,
            vk_state.render_finished_semaphore,
            nullptr
        );
        vk_state.render_finished_semaphore = VK_NULL_HANDLE;
    }
    if(vk_state.image_available_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(
            vk_state.device,
            vk_state.image_available_semaphore,
            nullptr
        );
        vk_state.image_available_semaphore = VK_NULL_HANDLE;
    }
}

void
cleanup_vulkan(void) {
    if(vk_state.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vk_state.device);
        cleanup_command_buffers();
        cleanup_pipeline();
        cleanup_swapchain();
        cleanup_frame_sync_objects();
        vkDestroyDevice(vk_state.device, nullptr);
        vk_state.device = VK_NULL_HANDLE;
    }

    if(vk_state.surface != VK_NULL_HANDLE &&
       vk_state.instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vk_state.instance, vk_state.surface, nullptr);
        vk_state.surface = VK_NULL_HANDLE;
    }

#ifndef NDEBUG
    if(vk_state.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                vk_state.instance,
                "vkDestroyDebugUtilsMessengerEXT"
            );
        if(destroy_debug_utils_messenger != nullptr) {
            destroy_debug_utils_messenger(
                vk_state.instance,
                vk_state.debug_messenger,
                nullptr
            );
        }
        vk_state.debug_messenger = VK_NULL_HANDLE;
    }
#endif

    if(vk_state.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(vk_state.instance, nullptr);
    }

    vk_state = {};
}

b32
init_vulkan(Arena *arena, GLFWwindow *window, u32 lane_count) {
    assert(arena != nullptr, "Vulkan arena must not be null!");
    assert(window != nullptr, "Vulkan window must not be null!");
    assert(lane_count > 0, "Lane count must be non-zero!");
    assert(lane_count <= MAX_LANES, "Lane count exceeds MAX_LANES!");

    b32 result = false;
    if(vk_state.initialized) {
        cleanup_vulkan();
    }

    vk_state.arena = arena;
    vk_state.window = window;
    vk_state.active_lane_count = lane_count;

    TemporaryMemory temporary_memory = begin_temporary_memory(arena);
    char const *layers[1] = {};
    u32 layer_count = 0;
    VkInstanceCreateInfo create_info = {};

#ifndef NDEBUG
    b32 has_validation_layer = false;
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
#endif

    VkApplicationInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    info.pApplicationName = "The Game";
    info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    info.pEngineName = "The Game";
    info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    info.apiVersion = get_target_api_version();
    vk_state.app_info = info;

    u32 extension_count = 0;
    char const **extensions = get_instance_extensions(arena, &extension_count);
    if(extension_count == 0 || extensions == nullptr) {
        LOG_FATAL(
            "glfwGetRequiredInstanceExtensions failed: %s",
            get_glfw_error_string()
        );
        goto cleanup;
    }

#ifndef NDEBUG
    has_validation_layer = has_layer(arena, validation_layer_name);
    if(has_validation_layer) {
        layers[layer_count++] = validation_layer_name;
    }
#endif

    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &vk_state.app_info;
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledLayerCount = layer_count;
    create_info.ppEnabledLayerNames = layer_count > 0 ? layers : nullptr;
    if(has_instance_extension(
           arena,
           VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
       )) {
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

#ifndef NDEBUG
    if(has_instance_extension(arena, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        build_debug_messenger_create_info(&debug_create_info);
        create_info.pNext = &debug_create_info;
    }
#endif

    if(vkCreateInstance(&create_info, nullptr, &vk_state.instance) !=
       VK_SUCCESS) {
        LOG_FATAL("Failed to create Vulkan instance.");
        goto cleanup;
    }

#ifndef NDEBUG
    if(has_instance_extension(arena, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) &&
       !create_debug_messenger()) {
        LOG_WARN("Failed to create Vulkan debug messenger.");
    }
#endif

    if(glfwCreateWindowSurface(
           vk_state.instance,
           window,
           nullptr,
           &vk_state.surface
       ) != VK_SUCCESS) {
        LOG_FATAL(
            "glfwCreateWindowSurface failed: %s",
            get_glfw_error_string()
        );
        goto cleanup;
    }

    if(!pick_physical_device(arena)) {
        LOG_FATAL(
            "Failed to find a Vulkan device w/ swapchain + dynamic rendering."
        );
        goto cleanup;
    }

    if(!create_device(arena)) {
        LOG_FATAL("Failed to create Vulkan logical device.");
        goto cleanup;
    }

    if(!load_dynamic_rendering_functions()) {
        LOG_FATAL("Failed to load Vulkan dynamic rendering functions.");
        goto cleanup;
    }

    if(!create_swapchain()) {
        LOG_FATAL("Failed to create Vulkan swapchain.");
        goto cleanup;
    }

    if(!create_pipeline()) {
        LOG_FATAL("Failed to create Vulkan pipeline.");
        goto cleanup;
    }

    if(!create_command_buffers(lane_count)) {
        LOG_FATAL("Failed to create Vulkan command buffers.");
        goto cleanup;
    }

    if(!create_frame_sync_objects()) {
        LOG_FATAL("Failed to create Vulkan sync objects.");
        goto cleanup;
    }

    vk_state.initialized = true;
    result = true;

cleanup:
    if(!result && vk_state.instance != VK_NULL_HANDLE) {
        cleanup_vulkan();
    }

    end_temporary_memory(temporary_memory);
    return result;
}
