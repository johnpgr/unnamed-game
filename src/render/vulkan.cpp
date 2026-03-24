#include "render/vulkan.h"

#define RGFW_VULKAN
#define RGFW_IMPORT
#include "third_party/rgfw/RGFW.h"
#undef RGFW_IMPORT
#undef RGFW_VULKAN

#include "base/base_arena.h"
#include "base/base_log.h"

#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define MAX_SWAPCHAIN_IMAGES 4

struct VulkanSpritePushConstants {
    vec2 center;
    vec2 size;
    vec4 color;
    vec2 screen_size;
};

struct VulkanState {
    Arena* arena;
    RGFW_window* window;

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
    volatile bool fatal_error;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    u32 swapchain_image_count;
    VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchain_views[MAX_SWAPCHAIN_IMAGES];

    VkCommandPool primary_pool;
    VkCommandBuffer primary_cmd;

    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphores[MAX_SWAPCHAIN_IMAGES];
    VkFence frame_fence;

    VkPipelineLayout pipeline_layout;
    VkPipeline sprite_pipeline;

    u32 frame_image_index;
    bool frame_active;

    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering;
    PFN_vkCmdEndRenderingKHR cmd_end_rendering;
};

global_variable VulkanState vk_state = {};

#ifndef NDEBUG
global_variable char const* validation_layer_name =
    "VK_LAYER_KHRONOS_validation";
#endif
global_variable char const* portability_subset_extension_name =
    "VK_KHR_portability_subset";

#ifndef ASSET_DIR
#error "ASSET_DIR must be defined by the build."
#endif

struct SwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;
    u32 format_count;
    VkPresentModeKHR* present_modes;
    u32 present_mode_count;
};

internal u32 get_target_api_version(void) {
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

internal bool has_instance_extension(Arena* arena, char const* extension_name) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(extension_name != nullptr, "Extension name must not be null!");

    u32 extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        nullptr
    );
    if(result != VK_SUCCESS || extension_count == 0) {
        return false;
    }

    Temp temporary_memory = temp_begin(arena);
    VkExtensionProperties* extensions =
        push_array(arena, VkExtensionProperties, extension_count);

    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        extensions
    );
    if(result != VK_SUCCESS) {
        temp_end(temporary_memory);
        return false;
    }

    bool found = false;
    for(u32 index = 0; index < extension_count; ++index) {
        if(strcmp(extensions[index].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    temp_end(temporary_memory);
    return found;
}

internal char const** get_instance_extensions(
    Arena* arena,
    u32* out_extension_count
) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(
        out_extension_count != nullptr,
        "Extension count output must not be null!"
    );

    size_t rgfw_extension_count = 0;
    char const** rgfw_extensions =
        RGFW_getRequiredInstanceExtensions_Vulkan(&rgfw_extension_count);
    if(rgfw_extensions == nullptr || rgfw_extension_count == 0) {
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

    u32 extension_count = (u32)rgfw_extension_count + extra_extension_count;
    char const** extensions = push_array(arena, char const*, extension_count);

    u32 extension_index = 0;
    for(size_t rgfw_index = 0; rgfw_index < rgfw_extension_count; ++rgfw_index) {
        extensions[extension_index++] = rgfw_extensions[rgfw_index];
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

internal bool has_layer(Arena* arena, char const* layer_name) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(layer_name != nullptr, "Layer name must not be null!");

    u32 layer_count = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    if(result != VK_SUCCESS || layer_count == 0) {
        return false;
    }

    Temp temporary_memory = temp_begin(arena);
    VkLayerProperties* layers =
        push_array(arena, VkLayerProperties, layer_count);

    result = vkEnumerateInstanceLayerProperties(&layer_count, layers);
    if(result != VK_SUCCESS) {
        temp_end(temporary_memory);
        return false;
    }

    bool found = false;
    for(u32 index = 0; index < layer_count; ++index) {
        if(strcmp(layers[index].layerName, layer_name) == 0) {
            found = true;
            break;
        }
    }

    temp_end(temporary_memory);
    return found;
}

#ifndef NDEBUG
internal VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    void* user_data
) {
    (void)message_types;
    (void)user_data;

    char const* message =
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

internal void build_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT* out_create_info
) {
    ASSERT(
        out_create_info != nullptr,
        "Debug messenger create info output must not be null!"
    );

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

internal bool create_debug_messenger(void) {
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

internal bool has_device_extension(
    Arena* arena,
    VkPhysicalDevice physical_device,
    char const* extension_name
) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(
        physical_device != VK_NULL_HANDLE,
        "Physical device must not be null!"
    );
    ASSERT(extension_name != nullptr, "Extension name must not be null!");

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

    Temp temporary_memory = temp_begin(arena);
    VkExtensionProperties* extensions =
        push_array(arena, VkExtensionProperties, extension_count);

    result = vkEnumerateDeviceExtensionProperties(
        physical_device,
        nullptr,
        &extension_count,
        extensions
    );
    if(result != VK_SUCCESS) {
        temp_end(temporary_memory);
        return false;
    }

    bool found = false;
    for(u32 index = 0; index < extension_count; ++index) {
        if(strcmp(extensions[index].extensionName, extension_name) == 0) {
            found = true;
            break;
        }
    }

    temp_end(temporary_memory);
    return found;
}

internal bool supports_dynamic_rendering(
    Arena* arena,
    VkPhysicalDevice physical_device
) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(
        physical_device != VK_NULL_HANDLE,
        "Physical device must not be null!"
    );

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    bool has_dynamic_rendering_extension = has_device_extension(
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

internal bool find_graphics_queue_family(
    Arena* arena,
    VkPhysicalDevice physical_device,
    u32* out_queue_family_index
) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(
        physical_device != VK_NULL_HANDLE,
        "Physical device must not be null!"
    );
    ASSERT(
        out_queue_family_index != nullptr,
        "Queue family index output must not be null!"
    );

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        nullptr
    );
    if(queue_family_count == 0) {
        return false;
    }

    Temp temporary_memory = temp_begin(arena);
    VkQueueFamilyProperties* queue_families =
        push_array(arena, VkQueueFamilyProperties, queue_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        queue_families
    );

    bool found = false;
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

    temp_end(temporary_memory);
    return found;
}

internal u32 score_device(Arena* arena, VkPhysicalDevice physical_device) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(
        physical_device != VK_NULL_HANDLE,
        "Physical device must not be null!"
    );

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

internal bool query_swapchain_support(
    Arena* arena,
    VkPhysicalDevice physical_device,
    SwapchainSupportInfo* out_info
) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(
        physical_device != VK_NULL_HANDLE,
        "Physical device must not be null!"
    );
    ASSERT(out_info != nullptr, "Swapchain support output must not be null!");

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
        push_array(arena, VkSurfaceFormatKHR, out_info->format_count);
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
        push_array(arena, VkPresentModeKHR, out_info->present_mode_count);

    return vkGetPhysicalDeviceSurfacePresentModesKHR(
               physical_device,
               vk_state.surface,
               &out_info->present_mode_count,
               out_info->present_modes
           ) == VK_SUCCESS;
}

internal VkSurfaceFormatKHR
choose_surface_format(SwapchainSupportInfo* support) {
    ASSERT(support != nullptr, "Swapchain support must not be null!");

    for(u32 index = 0; index < support->format_count; ++index) {
        VkSurfaceFormatKHR format = support->formats[index];
        if(format.format == VK_FORMAT_B8G8R8A8_UNORM &&
           format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return support->formats[0];
}

internal VkPresentModeKHR choose_present_mode(SwapchainSupportInfo* support) {
    ASSERT(support != nullptr, "Swapchain support must not be null!");

    for(u32 index = 0; index < support->present_mode_count; ++index) {
        if(support->present_modes[index] == VK_PRESENT_MODE_FIFO_KHR) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    return support->present_modes[0];
}

internal VkExtent2D choose_swapchain_extent(SwapchainSupportInfo* support) {
    ASSERT(support != nullptr, "Swapchain support must not be null!");

    if(support->capabilities.currentExtent.width != UINT32_MAX) {
        return support->capabilities.currentExtent;
    }

    i32 framebuffer_width = 0;
    i32 framebuffer_height = 0;
    RGFW_window_getSizeInPixels(
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

internal bool build_shader_path(
    char* buffer,
    u64 buffer_size,
    char const* file_name
) {
    ASSERT(buffer != nullptr, "Shader path buffer must not be null!");
    ASSERT(file_name != nullptr, "Shader file name must not be null!");

    int written =
        snprintf(buffer, buffer_size, "%s/shaders/%s", ASSET_DIR, file_name);
    return written > 0 && (u64)written < buffer_size;
}

internal void* read_binary_file(Arena* arena, char const* path, u64* out_size) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(path != nullptr, "File path must not be null!");
    ASSERT(out_size != nullptr, "Size output must not be null!");

    FILE* file = fopen(path, "rb");
    if(file == nullptr) {
        return nullptr;
    }

    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return nullptr;
    }

    long file_size = ftell(file);
    if(file_size <= 0) {
        fclose(file);
        return nullptr;
    }

    if(fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return nullptr;
    }

    void* data = push_size(arena, (u64)file_size);
    usize read_size = fread(data, 1, (usize)file_size, file);
    fclose(file);

    if(read_size != (usize)file_size) {
        return nullptr;
    }

    *out_size = (u64)file_size;
    return data;
}

internal bool create_shader_module(
    char const* file_name,
    VkShaderModule* out_shader_module
) {
    ASSERT(file_name != nullptr, "Shader file name must not be null!");
    ASSERT(
        out_shader_module != nullptr,
        "Shader module output must not be null!"
    );

    char shader_path[4096] = {};
    if(!build_shader_path(shader_path, sizeof(shader_path), file_name)) {
        LOG_ERROR("Failed to build shader path for %s.", file_name);
        return false;
    }

    Temp temporary_memory = temp_begin(vk_state.arena);
    u64 shader_size = 0;
    void* shader_data =
        read_binary_file(vk_state.arena, shader_path, &shader_size);
    if(shader_data == nullptr || shader_size == 0) {
        LOG_ERROR("Failed to read shader %s.", shader_path);
        temp_end(temporary_memory);
        return false;
    }

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader_size;
    create_info.pCode = (u32 const*)shader_data;

    bool result = vkCreateShaderModule(
                      vk_state.device,
                      &create_info,
                      nullptr,
                      out_shader_module
                  ) == VK_SUCCESS;
    temp_end(temporary_memory);
    return result;
}

internal void cleanup_pipeline(void) {
    if(vk_state.sprite_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk_state.device, vk_state.sprite_pipeline, nullptr);
        vk_state.sprite_pipeline = VK_NULL_HANDLE;
    }
    if(vk_state.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(
            vk_state.device,
            vk_state.pipeline_layout,
            nullptr
        );
        vk_state.pipeline_layout = VK_NULL_HANDLE;
    }
}

internal bool create_pipeline(void) {
    VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
    VkShaderModule fragment_shader_module = VK_NULL_HANDLE;
    bool result = false;
    VkPushConstantRange push_constant_range = {};
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    VkPipelineShaderStageCreateInfo shader_stages[2] = {};
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    VkPipelineViewportStateCreateInfo viewport_state = {};
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    VkPipelineColorBlendStateCreateInfo color_blending = {};
    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    VkPipelineRenderingCreateInfoKHR rendering_info = {};
    VkGraphicsPipelineCreateInfo pipeline_info = {};

    if(!create_shader_module("sprite.vert.spv", &vertex_shader_module)) {
        goto cleanup;
    }
    if(!create_shader_module("sprite.frag.spv", &fragment_shader_module)) {
        goto cleanup;
    }

    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(VulkanSpritePushConstants);

    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    if(vkCreatePipelineLayout(
           vk_state.device,
           &pipeline_layout_info,
           nullptr,
           &vk_state.pipeline_layout
       ) != VK_SUCCESS) {
        goto cleanup;
    }

    shader_stages[0].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vertex_shader_module;
    shader_stages[0].pName = "main";
    shader_stages[1].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = fragment_shader_module;
    shader_stages[1].pName = "main";

    vertex_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    color_blending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = ARRAY_COUNT(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &vk_state.swapchain_format;

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &rendering_info;
    pipeline_info.stageCount = ARRAY_COUNT(shader_stages);
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = vk_state.pipeline_layout;
    pipeline_info.renderPass = VK_NULL_HANDLE;
    pipeline_info.subpass = 0;

    if(vkCreateGraphicsPipelines(
           vk_state.device,
           VK_NULL_HANDLE,
           1,
           &pipeline_info,
           nullptr,
           &vk_state.sprite_pipeline
       ) != VK_SUCCESS) {
        goto cleanup;
    }

    result = true;

cleanup:
    if(fragment_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk_state.device, fragment_shader_module, nullptr);
    }
    if(vertex_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk_state.device, vertex_shader_module, nullptr);
    }
    if(!result) {
        cleanup_pipeline();
    }
    return result;
}

internal bool wait_for_nonzero_framebuffer(void) {
    i32 framebuffer_width = 0;
    i32 framebuffer_height = 0;

    while(framebuffer_width == 0 || framebuffer_height == 0) {
        if(RGFW_window_shouldClose(vk_state.window)) {
            return false;
        }

        RGFW_window_getSizeInPixels(
            vk_state.window,
            &framebuffer_width,
            &framebuffer_height
        );
        if(framebuffer_width == 0 || framebuffer_height == 0) {
            RGFW_waitForEvent(RGFW_eventWaitNext);
        }
    }

    return true;
}

internal bool create_swapchain(void) {
    Temp temporary_memory = temp_begin(vk_state.arena);
    SwapchainSupportInfo support = {};
    if(!query_swapchain_support(
           vk_state.arena,
           vk_state.physical_device,
           &support
       )) {
        temp_end(temporary_memory);
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
        temp_end(temporary_memory);
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
        temp_end(temporary_memory);
        return false;
    }

    if(vkGetSwapchainImagesKHR(
           vk_state.device,
           vk_state.swapchain,
           &image_count,
           vk_state.swapchain_images
       ) != VK_SUCCESS) {
        temp_end(temporary_memory);
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
            temp_end(temporary_memory);
            return false;
        }
    }

    temp_end(temporary_memory);
    return true;
}

internal void cleanup_swapchain(void) {
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

internal bool recreate_swapchain(void) {
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

internal bool create_command_buffers(void) {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = vk_state.graphics_queue_family_index;

    if(vkCreateCommandPool(
           vk_state.device,
           &pool_info,
           nullptr,
           &vk_state.primary_pool
       ) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo primary_alloc_info = {};
    primary_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    primary_alloc_info.commandPool = vk_state.primary_pool;
    primary_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    primary_alloc_info.commandBufferCount = 1;

    if(vkAllocateCommandBuffers(
           vk_state.device,
           &primary_alloc_info,
           &vk_state.primary_cmd
       ) != VK_SUCCESS) {
        return false;
    }

    return true;
}

internal void cleanup_command_buffers(void) {
    if(vk_state.primary_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vk_state.device, vk_state.primary_pool, nullptr);
        vk_state.primary_pool = VK_NULL_HANDLE;
        vk_state.primary_cmd = VK_NULL_HANDLE;
    }
}

internal vec4 get_clear_color(PushCmdBuffer* buffer) {
    ASSERT(buffer != nullptr, "Push command buffer must not be null!");

    vec4 result = vec4(0.04f, 0.05f, 0.08f, 1.0f);

    for(u32 offset = 0; offset < buffer->used;) {
        PushCmd* cmd = (PushCmd*)(buffer->base + offset);
        if(cmd->type == CmdType_Clear) {
            CmdClear* clear_cmd = (CmdClear*)cmd;
            result = clear_cmd->color;
            break;
        }

        offset += cmd->size;
    }

    return result;
}

internal void transition_swapchain_image(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlags src_access_mask,
    VkAccessFlags dst_access_mask,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dst_stage
) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        command_buffer,
        src_stage,
        dst_stage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}

internal bool pick_physical_device(Arena* arena) {
    ASSERT(arena != nullptr, "Arena must not be null!");

    u32 physical_device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(
        vk_state.instance,
        &physical_device_count,
        nullptr
    );
    if(result != VK_SUCCESS || physical_device_count == 0) {
        return false;
    }

    Temp temporary_memory = temp_begin(arena);
    VkPhysicalDevice* physical_devices =
        push_array(arena, VkPhysicalDevice, physical_device_count);

    result = vkEnumeratePhysicalDevices(
        vk_state.instance,
        &physical_device_count,
        physical_devices
    );
    if(result != VK_SUCCESS) {
        temp_end(temporary_memory);
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

    temp_end(temporary_memory);
    return vk_state.physical_device != VK_NULL_HANDLE;
}

internal bool create_device(Arena* arena) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(
        vk_state.physical_device != VK_NULL_HANDLE,
        "Physical device must not be null!"
    );

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

    bool needs_dynamic_rendering_extension =
        properties.apiVersion < VK_API_VERSION_1_3;
    bool needs_portability_subset_extension = has_device_extension(
        arena,
        vk_state.physical_device,
        portability_subset_extension_name
    );

    char const* device_extensions[3] = {};
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

internal bool load_dynamic_rendering_functions(void) {
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

internal bool create_frame_sync_objects(void) {
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

    for(u32 image_index = 0; image_index < MAX_SWAPCHAIN_IMAGES;
        ++image_index) {
        if(vkCreateSemaphore(
               vk_state.device,
               &semaphore_info,
               nullptr,
               &vk_state.render_finished_semaphores[image_index]
           ) != VK_SUCCESS) {
            return false;
        }
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

internal void cleanup_frame_sync_objects(void) {
    if(vk_state.frame_fence != VK_NULL_HANDLE) {
        vkDestroyFence(vk_state.device, vk_state.frame_fence, nullptr);
        vk_state.frame_fence = VK_NULL_HANDLE;
    }
    for(u32 image_index = 0; image_index < MAX_SWAPCHAIN_IMAGES;
        ++image_index) {
        if(vk_state.render_finished_semaphores[image_index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(
                vk_state.device,
                vk_state.render_finished_semaphores[image_index],
                nullptr
            );
            vk_state.render_finished_semaphores[image_index] = VK_NULL_HANDLE;
        }
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

bool begin_frame(void) {
    if(vk_state.fatal_error) {
        return false;
    }

    if(vkWaitForFences(
           vk_state.device,
           1,
           &vk_state.frame_fence,
           VK_TRUE,
           UINT64_MAX
       ) != VK_SUCCESS) {
        LOG_ERROR("vkWaitForFences failed.");
        vk_state.fatal_error = true;
        return false;
    }

    if(!wait_for_nonzero_framebuffer()) {
        vk_state.fatal_error = true;
        return false;
    }

    i32 framebuffer_width = 0;
    i32 framebuffer_height = 0;
    RGFW_window_getSizeInPixels(
        vk_state.window,
        &framebuffer_width,
        &framebuffer_height
    );
    if((u32)framebuffer_width != vk_state.swapchain_extent.width ||
       (u32)framebuffer_height != vk_state.swapchain_extent.height) {
        if(!recreate_swapchain()) {
            LOG_ERROR("Failed to recreate swapchain after resize.");
            vk_state.fatal_error = true;
            return false;
        }
    }

    for(;;) {
        VkResult acquire_result = vkAcquireNextImageKHR(
            vk_state.device,
            vk_state.swapchain,
            UINT64_MAX,
            vk_state.image_available_semaphore,
            VK_NULL_HANDLE,
            &vk_state.frame_image_index
        );

        if(acquire_result == VK_SUCCESS ||
           acquire_result == VK_SUBOPTIMAL_KHR) {
            break;
        }

        if(acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
            if(!recreate_swapchain()) {
                LOG_ERROR("Failed to recreate swapchain after acquire.");
                vk_state.fatal_error = true;
                return false;
            }
            continue;
        }

        LOG_ERROR("vkAcquireNextImageKHR failed.");
        vk_state.fatal_error = true;
        return false;
    }

    if(vkResetFences(vk_state.device, 1, &vk_state.frame_fence) != VK_SUCCESS) {
        LOG_ERROR("vkResetFences failed.");
        vk_state.fatal_error = true;
        return false;
    }

    if(vkResetCommandPool(vk_state.device, vk_state.primary_pool, 0) !=
       VK_SUCCESS) {
        LOG_ERROR("vkResetCommandPool failed for primary pool.");
        vk_state.fatal_error = true;
        return false;
    }

    vk_state.frame_active = true;
    return true;
}

bool render_drain_cmd_buffer(PushCmdBuffer* buffer) {
    ASSERT(buffer != nullptr, "Push command buffer must not be null!");

    if(!vk_state.frame_active || vk_state.fatal_error) {
        return !vk_state.fatal_error;
    }

    VkSemaphore render_finished_semaphore =
        vk_state.render_finished_semaphores[vk_state.frame_image_index];

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if(vkBeginCommandBuffer(vk_state.primary_cmd, &begin_info) != VK_SUCCESS) {
        LOG_ERROR("vkBeginCommandBuffer failed for primary command buffer.");
        return false;
    }

    VkImage swapchain_image =
        vk_state.swapchain_images[vk_state.frame_image_index];
    transition_swapchain_image(
        vk_state.primary_cmd,
        swapchain_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );

    vec4 clear_color = get_clear_color(buffer);
    VkClearValue clear_value = {};
    clear_value.color.float32[0] = clear_color.r;
    clear_value.color.float32[1] = clear_color.g;
    clear_value.color.float32[2] = clear_color.b;
    clear_value.color.float32[3] = clear_color.a;

    VkRenderingAttachmentInfoKHR color_attachment = {};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color_attachment.imageView =
        vk_state.swapchain_views[vk_state.frame_image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue = clear_value;

    VkRenderingInfoKHR rendering_info = {};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    rendering_info.renderArea.extent = vk_state.swapchain_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    vk_state.cmd_begin_rendering(vk_state.primary_cmd, &rendering_info);

    vkCmdBindPipeline(
        vk_state.primary_cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        vk_state.sprite_pipeline
    );

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)vk_state.swapchain_extent.width;
    viewport.height = (f32)vk_state.swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vk_state.primary_cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = vk_state.swapchain_extent;
    vkCmdSetScissor(vk_state.primary_cmd, 0, 1, &scissor);

    vec2 screen_size = vec2(
        (f32)vk_state.swapchain_extent.width,
        (f32)vk_state.swapchain_extent.height
    );

    for(u32 offset = 0; offset < buffer->used;) {
        PushCmd* cmd = (PushCmd*)(buffer->base + offset);
        if(cmd->type == CmdType_Rect) {
            CmdRect* rect_cmd = (CmdRect*)cmd;
            VulkanSpritePushConstants push_constants = {};
            push_constants.center = rect_cmd->center;
            push_constants.size = rect_cmd->size;
            push_constants.color = rect_cmd->color;
            push_constants.screen_size = screen_size;

            vkCmdPushConstants(
                vk_state.primary_cmd,
                vk_state.pipeline_layout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(push_constants),
                &push_constants
            );
            vkCmdDraw(vk_state.primary_cmd, 6, 1, 0, 0);
        }

        offset += cmd->size;
    }

    vk_state.cmd_end_rendering(vk_state.primary_cmd);

    transition_swapchain_image(
        vk_state.primary_cmd,
        swapchain_image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    );

    if(vkEndCommandBuffer(vk_state.primary_cmd) != VK_SUCCESS) {
        LOG_ERROR("vkEndCommandBuffer failed for primary command buffer.");
        return false;
    }

    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &vk_state.image_available_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk_state.primary_cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_semaphore;

    if(vkQueueSubmit(
           vk_state.graphics_queue,
           1,
           &submit_info,
           vk_state.frame_fence
       ) != VK_SUCCESS) {
        LOG_ERROR("vkQueueSubmit failed.");
        return false;
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vk_state.swapchain;
    present_info.pImageIndices = &vk_state.frame_image_index;

    VkResult present_result =
        vkQueuePresentKHR(vk_state.graphics_queue, &present_info);
    if(present_result == VK_ERROR_OUT_OF_DATE_KHR ||
       present_result == VK_SUBOPTIMAL_KHR) {
        if(!recreate_swapchain()) {
            LOG_ERROR("Failed to recreate swapchain after present.");
            return false;
        }
    } else if(present_result != VK_SUCCESS) {
        LOG_ERROR("vkQueuePresentKHR failed.");
        return false;
    }

    vk_state.frame_active = false;
    return true;
}

void cleanup_vulkan(void) {
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

bool init_vulkan(Arena* arena, RGFW_window* window) {
    ASSERT(arena != nullptr, "Vulkan arena must not be null!");
    ASSERT(window != nullptr, "Vulkan window must not be null!");

    bool result = false;
    if(vk_state.initialized) {
        cleanup_vulkan();
    }

    vk_state.arena = arena;
    vk_state.window = window;

    Temp temporary_memory = temp_begin(arena);
    char const* layers[1] = {};
    u32 layer_count = 0;
    VkInstanceCreateInfo create_info = {};

#ifndef NDEBUG
    bool has_validation_layer = false;
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
    char const** extensions = get_instance_extensions(arena, &extension_count);
    if(extension_count == 0 || extensions == nullptr) {
        LOG_FATAL("RGFW_getRequiredInstanceExtensions_Vulkan failed.");
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

    if(RGFW_window_createSurface_Vulkan(
           window,
           vk_state.instance,
           &vk_state.surface
       ) != VK_SUCCESS) {
        LOG_FATAL("RGFW_window_createSurface_Vulkan failed.");
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

    if(!create_command_buffers()) {
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

    temp_end(temporary_memory);
    return result;
}
