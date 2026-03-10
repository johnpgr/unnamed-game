struct VulkanState {
    VkInstance instance;
    VkApplicationInfo app_info;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    u32 graphics_queue_family_index;
    VkDebugUtilsMessengerEXT debug_messenger;
    bool initialized;
};

internal VulkanState vk_state = {};

#ifndef NDEBUG
internal const char* renderer_vulkan_validation_layer_name =
    "VK_LAYER_KHRONOS_validation";
#endif
internal const char* renderer_vulkan_portability_subset_extension_name =
    "VK_KHR_portability_subset";

internal bool renderer_vulkan_find_graphics_queue_family(
    Arena* arena,
    VkPhysicalDevice physical_device,
    u32* out_queue_family_index
);

internal u32 renderer_vulkan_target_api_version(void);

internal u32
renderer_vulkan_device_score(Arena* arena, VkPhysicalDevice physical_device);

internal u32 renderer_vulkan_target_api_version(void) {
    u32 api_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion enumerate_instance_version =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(
            VK_NULL_HANDLE,
            "vkEnumerateInstanceVersion"
        );

    if (enumerate_instance_version != nullptr &&
        enumerate_instance_version(&api_version) == VK_SUCCESS) {
        return api_version;
    }

    return VK_API_VERSION_1_0;
}

internal bool renderer_vulkan_is_ext_available(
    Arena* arena,
    const char* extension_name
) {
    if (arena == nullptr || extension_name == nullptr ||
        extension_name[0] == 0) {
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

    u64 arena_mark = arena->mark();
    VkExtensionProperties* extensions =
        arena->push<VkExtensionProperties>(extension_count);

    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        extensions
    );
    if (result != VK_SUCCESS) {
        arena->restore(arena_mark);
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    arena->restore(arena_mark);
    return found;
}

internal Array<const char*> renderer_vulkan_instance_extensions(Arena* arena) {
    ASSUME(arena != nullptr);

    ArrayList<const char*> extensions =
        platform_vulkan_get_instance_extensions(arena);

#ifndef NDEBUG
    if (renderer_vulkan_is_ext_available(
            arena,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        )) {
        extensions.push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    return extensions.to_array();
}

internal bool renderer_vulkan_is_layer_available(
    Arena* arena,
    const char* layer_name
) {
    if (arena == nullptr || layer_name == nullptr || layer_name[0] == 0) {
        return false;
    }

    u32 layer_count = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    if (result != VK_SUCCESS || layer_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    VkLayerProperties* layers = arena->push<VkLayerProperties>(layer_count);

    result = vkEnumerateInstanceLayerProperties(&layer_count, layers);
    if (result != VK_SUCCESS) {
        arena->restore(arena_mark);
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < layer_count; i++) {
        if (strcmp(layers[i].layerName, layer_name) == 0) {
            found = true;
            break;
        }
    }

    arena->restore(arena_mark);
    return found;
}

#ifndef NDEBUG
internal VKAPI_ATTR VkBool32 VKAPI_CALL renderer_vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
) {
    (void)message_types;
    (void)user_data;

    const char* message =
        (callback_data != nullptr && callback_data->pMessage != nullptr)
            ? callback_data->pMessage
            : "Unknown Vulkan validation message";

    if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) !=
        0) {
        log_error("Vulkan: %s", message);
    } else if (
        (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) !=
        0
    ) {
        log_warn("Vulkan: %s", message);
    } else if (
        (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0
    ) {
        log_info("Vulkan: %s", message);
    } else {
        log_debug("Vulkan: %s", message);
    }

    return VK_FALSE;
}

internal void renderer_vulkan_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT* out_create_info
) {
    ASSUME(out_create_info != nullptr);

    *out_create_info = {};
    out_create_info->sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    out_create_info->messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    out_create_info->messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    out_create_info->pfnUserCallback = renderer_vulkan_debug_callback;
}

internal bool renderer_vulkan_create_debug_messenger(void) {
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            vk_state.instance,
            "vkCreateDebugUtilsMessengerEXT"
        );
    if (create_debug_utils_messenger == nullptr) {
        return false;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    renderer_vulkan_debug_messenger_create_info(&create_info);

    return create_debug_utils_messenger(
               vk_state.instance,
               &create_info,
               nullptr,
               &vk_state.debug_messenger
           ) == VK_SUCCESS;
}
#endif

internal bool renderer_vulkan_is_device_ext_available(
    Arena* arena,
    VkPhysicalDevice physical_device,
    const char* extension_name
) {
    if (arena == nullptr || physical_device == VK_NULL_HANDLE ||
        extension_name == nullptr || extension_name[0] == 0) {
        return false;
    }

    u32 extension_count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(
        physical_device,
        nullptr,
        &extension_count,
        nullptr
    );
    if (result != VK_SUCCESS || extension_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    VkExtensionProperties* extensions =
        arena->push<VkExtensionProperties>(extension_count);

    result = vkEnumerateDeviceExtensionProperties(
        physical_device,
        nullptr,
        &extension_count,
        extensions
    );
    if (result != VK_SUCCESS) {
        arena->restore(arena_mark);
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    arena->restore(arena_mark);
    return found;
}

internal bool renderer_vulkan_supports_dynamic_rendering(
    Arena* arena,
    VkPhysicalDevice physical_device
) {
    if (physical_device == VK_NULL_HANDLE) {
        return false;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    bool has_dynamic_rendering_extension =
        renderer_vulkan_is_device_ext_available(
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

internal bool vulkan_renderer_is_device_suitable(
    Arena* arena,
    VkPhysicalDevice physical_device
) {
    return renderer_vulkan_device_score(arena, physical_device) > 0;
}

internal u32
renderer_vulkan_device_score(Arena* arena, VkPhysicalDevice physical_device) {
    if (arena == nullptr || physical_device == VK_NULL_HANDLE) {
        return 0;
    }

    if (!renderer_vulkan_supports_dynamic_rendering(arena, physical_device)) {
        return 0;
    }

    u32 graphics_queue_family_index = 0;
    if (!renderer_vulkan_find_graphics_queue_family(
            arena,
            physical_device,
            &graphics_queue_family_index
        )) {
        return 0;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    VkPhysicalDeviceFeatures features = {};
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    u32 score = 0;

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    if (features.geometryShader == VK_TRUE) {
        score += 100;
    }

    score += properties.limits.maxImageDimension2D;
    return score;
}

internal bool renderer_vulkan_find_graphics_queue_family(
    Arena* arena,
    VkPhysicalDevice physical_device,
    u32* out_queue_family_index
) {
    if (arena == nullptr || physical_device == VK_NULL_HANDLE ||
        out_queue_family_index == nullptr) {
        return false;
    }

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        nullptr
    );
    if (queue_family_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    VkQueueFamilyProperties* queue_families =
        arena->push<VkQueueFamilyProperties>(queue_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        queue_families
    );

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

    arena->restore(arena_mark);
    return found;
}

internal bool renderer_vulkan_pick_physical_device(Arena* arena) {
    ASSUME(arena != nullptr);

    u32 physical_device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(
        vk_state.instance,
        &physical_device_count,
        nullptr
    );
    if (result != VK_SUCCESS || physical_device_count == 0) {
        return false;
    }

    u64 arena_mark = arena->mark();
    VkPhysicalDevice* physical_devices =
        arena->push<VkPhysicalDevice>(physical_device_count);

    result = vkEnumeratePhysicalDevices(
        vk_state.instance,
        &physical_device_count,
        physical_devices
    );
    if (result != VK_SUCCESS) {
        arena->restore(arena_mark);
        return false;
    }

    vk_state.physical_device = VK_NULL_HANDLE;
    u32 best_score = 0;

    for (u32 i = 0; i < physical_device_count; i++) {
        if (!vulkan_renderer_is_device_suitable(arena, physical_devices[i])) {
            continue;
        }

        u32 score = renderer_vulkan_device_score(arena, physical_devices[i]);
        if (score <= best_score) {
            continue;
        }

        best_score = score;
        vk_state.physical_device = physical_devices[i];
    }

    arena->restore(arena_mark);
    return vk_state.physical_device != VK_NULL_HANDLE;
}

internal bool renderer_vulkan_create_device(Arena* arena) {
    ASSUME(arena != nullptr);

    if (vk_state.physical_device == VK_NULL_HANDLE) {
        return false;
    }

    if (!renderer_vulkan_find_graphics_queue_family(
            arena,
            vk_state.physical_device,
            &vk_state.graphics_queue_family_index
        )) {
        return false;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(vk_state.physical_device, &properties);

    float graphics_queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = vk_state.graphics_queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &graphics_queue_priority;

    bool needs_dynamic_rendering_extension =
        properties.apiVersion < VK_API_VERSION_1_3;
    bool needs_portability_subset_extension =
        renderer_vulkan_is_device_ext_available(
            arena,
            vk_state.physical_device,
            renderer_vulkan_portability_subset_extension_name
        );

    if (needs_dynamic_rendering_extension &&
        !renderer_vulkan_is_device_ext_available(
            arena,
            vk_state.physical_device,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
        )) {
        return false;
    }

    u32 device_extension_count = 0;
    if (needs_dynamic_rendering_extension) {
        device_extension_count += 1;
    }
    if (needs_portability_subset_extension) {
        device_extension_count += 1;
    }

    Array<const char*> device_extensions = {};
    if (device_extension_count > 0) {
        device_extensions =
            Array<const char*>::make(arena, device_extension_count);

        if (needs_dynamic_rendering_extension) {
            device_extensions.items[device_extensions.count++] =
                VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
        }

        if (needs_portability_subset_extension) {
            device_extensions.items[device_extensions.count++] =
                renderer_vulkan_portability_subset_extension_name;
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
    if (properties.apiVersion >= VK_API_VERSION_1_3) {
        create_info.pNext = &features13;
    } else {
        create_info.pNext = &dynamic_rendering_features;
    }

    if (vkCreateDevice(
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

internal void renderer_vulkan_cleanup() {
    if (vk_state.device != VK_NULL_HANDLE) {
        vkDestroyDevice(vk_state.device, nullptr);
    }
#ifndef NDEBUG
    if (vk_state.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                vk_state.instance,
                "vkDestroyDebugUtilsMessengerEXT"
            );
        if (destroy_debug_utils_messenger != nullptr) {
            destroy_debug_utils_messenger(
                vk_state.instance,
                vk_state.debug_messenger,
                nullptr
            );
        }
    }
#endif
    vkDestroyInstance(vk_state.instance, nullptr);
    vk_state = {};
}

internal bool renderer_vulkan_init(Arena* arena) {
    ASSUME(arena != nullptr);

    if (vk_state.initialized) {
        renderer_vulkan_cleanup();
    }

    VkApplicationInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    info.pApplicationName = "Unammed Game";
    info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    info.pEngineName = "No Engine";
    info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    info.apiVersion = renderer_vulkan_target_api_version();
    vk_state.app_info = info;

    Array<const char*> extensions = renderer_vulkan_instance_extensions(arena);
    Array<const char*> layers = {};
#ifndef NDEBUG
    bool has_validation_layer = renderer_vulkan_is_layer_available(
        arena,
        renderer_vulkan_validation_layer_name
    );
    if (has_validation_layer) {
        layers = Array<const char*>::make(arena, 1);
        layers.items[0] = renderer_vulkan_validation_layer_name;
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
    if (renderer_vulkan_is_ext_available(
            arena,
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        )) {
        create_info.flags |=
            VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
    if (renderer_vulkan_is_ext_available(
            arena,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        )) {
        renderer_vulkan_debug_messenger_create_info(&debug_create_info);
        create_info.pNext = &debug_create_info;
    }
#endif

    if (vkCreateInstance(&create_info, nullptr, &vk_state.instance) !=
        VK_SUCCESS) {
        log_fatal("Failed to create VK Instance!");
        return false;
    }

#ifndef NDEBUG
    if (renderer_vulkan_is_ext_available(
            arena,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        ) &&
        !renderer_vulkan_create_debug_messenger()) {
        log_warn("Failed to create Vulkan debug messenger.");
    }
#endif

    if (!renderer_vulkan_pick_physical_device(arena)) {
        log_fatal(
            "Failed to find a physical device with dynamic rendering support!"
        );
        return false;
    }

    if (!renderer_vulkan_create_device(arena)) {
        log_fatal("Failed to create Vulkan logical device!");
        return false;
    }

    vk_state.initialized = true;
    return true;
}

internal bool renderer_vulkan_draw_frame() {
    return true;
}
