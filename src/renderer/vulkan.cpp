#include "base/array.h"
#include "base/core.h"
#include "base/log.h"

#include <SDL3/SDL_vulkan.h>
#include <cstring>
#include <vulkan/vulkan.h>

#include "renderer/vulkan.h"

struct VulkanState {
    VkInstance instance;
    VkApplicationInfo app_info;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkSurfaceKHR surface;
    u32 graphics_queue_family_index;
    VkDebugUtilsMessengerEXT debug_messenger;
    bool dynamic_rendering_supported;
    bool initialized;
};

static VulkanState vk_state = {};

#ifndef NDEBUG
static const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
#endif
static const char* PORTABILITY_SUBSET_EXTENSION_NAME = "VK_KHR_portability_subset";

static bool FindGraphicsQueueFamily(
    Arena* arena,
    VkPhysicalDevice physical_device,
    u32* out_queue_family_index
);

static u32 GetTargetApiVersion(void);

static u32 ScoreDevice(Arena* arena, VkPhysicalDevice physical_device);

static u32 GetTargetApiVersion(void) {
    u32 api_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion enumerate_instance_version = (PFN_vkEnumerateInstanceVersion)
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");

    if (enumerate_instance_version != nullptr &&
        enumerate_instance_version(&api_version) == VK_SUCCESS) {
        return api_version;
    }

    return VK_API_VERSION_1_0;
}

static bool HasInstanceExtension(Arena* arena, const char* extension_name) {
    assume(arena != nullptr);
    assume(extension_name != nullptr);

    u32 extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    if (result != VK_SUCCESS || extension_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    defer {
        arena->restore(arena_mark);
    };

    VkExtensionProperties* extensions = arena->push<VkExtensionProperties>(extension_count);

    result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions);
    if (result != VK_SUCCESS) {
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    return found;
}

static Array<const char*> GetInstanceExtensions(Arena* arena) {
    assume(arena != nullptr);

    Uint32 sdl_extension_count = 0;
    char const* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    if (sdl_extensions == nullptr || sdl_extension_count == 0) {
        Array<const char*> empty_extensions = {};
        return empty_extensions;
    }

    ArrayList<const char*> extensions = ArrayListCreate<const char*>(arena);
    for (Uint32 i = 0; i < sdl_extension_count; i++) {
        extensions.push(sdl_extensions[i]);
    }

    if (HasInstanceExtension(arena, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        extensions.push(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }

#ifndef NDEBUG
    if (HasInstanceExtension(arena, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        extensions.push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    return extensions.toArray();
}

static bool HasLayer(Arena* arena, const char* layer_name) {
    assume(arena != nullptr);
    assume(layer_name != nullptr);

    u32 layer_count = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    if (result != VK_SUCCESS || layer_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    defer {
        arena->restore(arena_mark);
    };

    VkLayerProperties* layers = arena->push<VkLayerProperties>(layer_count);

    result = vkEnumerateInstanceLayerProperties(&layer_count, layers);
    if (result != VK_SUCCESS) {
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < layer_count; i++) {
        if (strcmp(layers[i].layerName, layer_name) == 0) {
            found = true;
            break;
        }
    }

    return found;
}

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
) {
    (void)message_types;
    (void)user_data;

    const char* message = (callback_data != nullptr && callback_data->pMessage != nullptr)
                              ? callback_data->pMessage
                              : "Unknown Vulkan validation message";

    if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        LOG_ERROR("Vulkan: %s", message);
    } else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        LOG_WARN("Vulkan: %s", message);
    } else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0) {
        LOG_INFO("Vulkan: %s", message);
    } else {
        LOG_DEBUG("Vulkan: %s", message);
    }

    return VK_FALSE;
}

static void BuildDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* out_create_info) {
    assume(out_create_info != nullptr);

    *out_create_info = {};
    out_create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    out_create_info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    out_create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    out_create_info->pfnUserCallback = DebugCallback;
}

static bool CreateDebugMessenger(void) {
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(vk_state.instance, "vkCreateDebugUtilsMessengerEXT");
    if (create_debug_utils_messenger == nullptr) {
        return false;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    BuildDebugMessengerCreateInfo(&create_info);

    return create_debug_utils_messenger(
               vk_state.instance,
               &create_info,
               nullptr,
               &vk_state.debug_messenger
           ) == VK_SUCCESS;
}
#endif

static bool HasDeviceExtension(
    Arena* arena,
    VkPhysicalDevice physical_device,
    const char* extension_name
) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);
    assume(extension_name != nullptr);

    u32 extension_count = 0;
    VkResult result =
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    if (result != VK_SUCCESS || extension_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    defer {
        arena->restore(arena_mark);
    };

    VkExtensionProperties* extensions = arena->push<VkExtensionProperties>(extension_count);

    result = vkEnumerateDeviceExtensionProperties(
        physical_device,
        nullptr,
        &extension_count,
        extensions
    );
    if (result != VK_SUCCESS) {
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    return found;
}

static bool SupportsDynamicRendering(Arena* arena, VkPhysicalDevice physical_device) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    bool has_dynamic_rendering_extension =
        HasDeviceExtension(arena, physical_device, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {};
    dynamic_rendering_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    if (properties.apiVersion >= VK_API_VERSION_1_3) {
        features2.pNext = &features13;
    } else if (has_dynamic_rendering_extension) {
        features2.pNext = &dynamic_rendering_features;
    }

    vkGetPhysicalDeviceFeatures2(physical_device, &features2);

    if (properties.apiVersion >= VK_API_VERSION_1_3) {
        return features13.dynamicRendering == VK_TRUE;
    }

    return has_dynamic_rendering_extension &&
           dynamic_rendering_features.dynamicRendering == VK_TRUE;
}

static bool IsDeviceSuitable(Arena* arena, VkPhysicalDevice physical_device) {
    return ScoreDevice(arena, physical_device) > 0;
}

static u32 ScoreDevice(Arena* arena, VkPhysicalDevice physical_device) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);

    // Check if the device has a graphics queue family that supports presenting to our surface
    u32 graphics_queue_family_index = 0;
    if (!FindGraphicsQueueFamily(arena, physical_device, &graphics_queue_family_index)) {
        return 0;
    }

    VkBool32 supports_device_surface = VK_FALSE;
    VkResult surface_support_result = vkGetPhysicalDeviceSurfaceSupportKHR(
        physical_device,
        graphics_queue_family_index,
        vk_state.surface,
        &supports_device_surface
    );
    if (surface_support_result != VK_SUCCESS) {
        return 0;
    }

    // If the device doesn't support presenting to the surface, it's not suitable
    if (!supports_device_surface) {
        return 0;
    }

    // Score the device based on its type and features.
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    // Discrete GPUs are preferred over integrated ones
    VkPhysicalDeviceFeatures features = {};
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    u32 score = 0;

    // Discrete GPUs are preferred over integrated ones
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Geometry shader support is preferred, but not required
    if (features.geometryShader == VK_TRUE) {
        score += 100;
    }

    // The maximum 2D image dimension is a rough indicator of the GPU's capabilities
    // so we can use it to further differentiate between devices.
    score += properties.limits.maxImageDimension2D;
    return score;
}

static bool FindGraphicsQueueFamily(
    Arena* arena,
    VkPhysicalDevice physical_device,
    u32* out_queue_family_index
) {
    assume(arena != nullptr);
    assume(physical_device != VK_NULL_HANDLE);
    assume(out_queue_family_index != nullptr);

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    if (queue_family_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    defer {
        arena->restore(arena_mark);
    };

    VkQueueFamilyProperties* queue_families =
        arena->push<VkQueueFamilyProperties>(queue_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    bool found = false;
    for (u32 i = 0; i < queue_family_count; i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
            queue_families[i].queueCount == 0) {
            continue;
        }

        *out_queue_family_index = i;
        found = true;
        break;
    }

    return found;
}

static bool PickPhysicalDevice(Arena* arena) {
    assume(arena != nullptr);

    u32 physical_device_count = 0;
    VkResult result =
        vkEnumeratePhysicalDevices(vk_state.instance, &physical_device_count, nullptr);
    if (result != VK_SUCCESS || physical_device_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    defer {
        arena->restore(arena_mark);
    };

    VkPhysicalDevice* physical_devices = arena->push<VkPhysicalDevice>(physical_device_count);

    result =
        vkEnumeratePhysicalDevices(vk_state.instance, &physical_device_count, physical_devices);
    if (result != VK_SUCCESS) {
        return false;
    }

    vk_state.physical_device = VK_NULL_HANDLE;
    u32 best_score = 0;

    for (u32 i = 0; i < physical_device_count; i++) {
        if (!IsDeviceSuitable(arena, physical_devices[i])) {
            continue;
        }

        u32 score = ScoreDevice(arena, physical_devices[i]);
        if (score <= best_score) {
            continue;
        }

        best_score = score;
        vk_state.physical_device = physical_devices[i];
    }

    return vk_state.physical_device != VK_NULL_HANDLE;
}

static bool CreateDevice(Arena* arena) {
    assume(arena != nullptr);

    if (vk_state.physical_device == VK_NULL_HANDLE) {
        return false;
    }

    if (!FindGraphicsQueueFamily(
            arena,
            vk_state.physical_device,
            &vk_state.graphics_queue_family_index
        )) {
        return false;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(vk_state.physical_device, &properties);

    vk_state.dynamic_rendering_supported =
        SupportsDynamicRendering(arena, vk_state.physical_device);

    float graphics_queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = vk_state.graphics_queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &graphics_queue_priority;

    bool needs_dynamic_rendering_extension =
        vk_state.dynamic_rendering_supported && properties.apiVersion < VK_API_VERSION_1_3;
    bool needs_portability_subset_extension =
        HasDeviceExtension(arena, vk_state.physical_device, PORTABILITY_SUBSET_EXTENSION_NAME);

    u32 device_extension_count = 0;
    if (needs_dynamic_rendering_extension) {
        device_extension_count += 1;
    }
    if (needs_portability_subset_extension) {
        device_extension_count += 1;
    }

    Array<const char*> device_extensions = {};
    if (device_extension_count > 0) {
        device_extensions = ArrayCreate<const char*>(arena, device_extension_count);

        if (needs_dynamic_rendering_extension) {
            device_extensions.items[device_extensions.count++] =
                VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
        }

        if (needs_portability_subset_extension) {
            device_extensions.items[device_extensions.count++] = PORTABILITY_SUBSET_EXTENSION_NAME;
        }
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
    create_info.enabledExtensionCount = device_extensions.count;
    create_info.ppEnabledExtensionNames = device_extensions.items;
    if (vk_state.dynamic_rendering_supported && properties.apiVersion >= VK_API_VERSION_1_3) {
        create_info.pNext = &features13;
    } else if (needs_dynamic_rendering_extension) {
        create_info.pNext = &dynamic_rendering_features;
    }

    if (vkCreateDevice(vk_state.physical_device, &create_info, nullptr, &vk_state.device) !=
        VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(
        vk_state.device,
        vk_state.graphics_queue_family_index,
        0,
        &vk_state.graphics_queue
    );

    return vk_state.device != VK_NULL_HANDLE && vk_state.graphics_queue != VK_NULL_HANDLE;
}

void CleanupVulkan(void) {
    if (vk_state.device != VK_NULL_HANDLE) {
        vkDestroyDevice(vk_state.device, nullptr);
    }
    if (vk_state.surface != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(vk_state.instance, vk_state.surface, nullptr);
    }
#ifndef NDEBUG
    if (vk_state.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(vk_state.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_debug_utils_messenger != nullptr) {
            destroy_debug_utils_messenger(vk_state.instance, vk_state.debug_messenger, nullptr);
        }
    }
#endif
    vkDestroyInstance(vk_state.instance, nullptr);
    vk_state = {};
}

bool InitVulkan(Arena* arena, SDL_Window* window) {
    assume(arena != nullptr);
    assume(window != nullptr);

    if (vk_state.initialized) {
        CleanupVulkan();
    }

    bool created_renderer = false;
    defer {
        if (!created_renderer && vk_state.instance != VK_NULL_HANDLE) {
            CleanupVulkan();
        }
    };

    VkApplicationInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    info.pApplicationName = "Unnamed Game";
    info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    info.pEngineName = "No Engine";
    info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    info.apiVersion = GetTargetApiVersion();
    vk_state.app_info = info;

    Array<const char*> extensions = GetInstanceExtensions(arena);
    if (extensions.count == 0 || extensions.items == nullptr) {
        LOG_FATAL("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
        return false;
    }
    Array<const char*> layers = {};

#ifndef NDEBUG
    bool has_validation_layer = HasLayer(arena, VALIDATION_LAYER_NAME);
    if (has_validation_layer) {
        layers = ArrayCreate<const char*>(arena, 1);
        layers.items[0] = VALIDATION_LAYER_NAME;
        layers.count = 1;
    }
#endif

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &vk_state.app_info;
    create_info.enabledExtensionCount = (u32)extensions.count;
    create_info.ppEnabledExtensionNames = extensions.items;
    create_info.enabledLayerCount = layers.count;
    create_info.ppEnabledLayerNames = layers.items;
    if (HasInstanceExtension(arena, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
    if (HasInstanceExtension(arena, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        BuildDebugMessengerCreateInfo(&debug_create_info);
        create_info.pNext = &debug_create_info;
    }
#endif

    if (vkCreateInstance(&create_info, nullptr, &vk_state.instance) != VK_SUCCESS) {
        LOG_FATAL("Failed to create VK Instance!");
        return false;
    }

#ifndef NDEBUG
    if (HasInstanceExtension(arena, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) && !CreateDebugMessenger()) {
        LOG_WARN("Failed to create Vulkan debug messenger.");
    }
#endif

    // The surface must be created after the instance and before picking a physical device
    if (!SDL_Vulkan_CreateSurface(window, vk_state.instance, nullptr, &vk_state.surface)) {
        LOG_FATAL("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return false;
    }

    if (!PickPhysicalDevice(arena)) {
        LOG_FATAL("Failed to find a Vulkan physical device that can present to the window!");
        return false;
    }

    if (!CreateDevice(arena)) {
        LOG_FATAL("Failed to create Vulkan logical device!");
        return false;
    }

    vk_state.initialized = true;
    created_renderer = true;
    return true;
}

bool Draw(void) {
    return true;
}
