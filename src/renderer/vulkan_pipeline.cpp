#include "renderer/vulkan.h"

#include "base/arena.h"
#include "base/log.h"

#include <cstring>
#include <cstdio>

#if OS_MAC
#include <mach-o/dyld.h>
#elif OS_LINUX
#include <unistd.h>
#endif

internal b32
get_executable_directory_for_renderer(char *buffer, u64 buffer_size) {
    assert(buffer != nullptr, "Executable path buffer must not be null!");
    assert(buffer_size > 0, "Executable path buffer must not be empty!");

#if OS_MAC
    u32 path_size = (u32)buffer_size;
    if(_NSGetExecutablePath(buffer, &path_size) != 0) {
        return false;
    }
    buffer[buffer_size - 1] = 0;
#elif OS_LINUX
    ssize_t size_read = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if(size_read <= 0) {
        return false;
    }
    buffer[size_read] = 0;
#else
    return false;
#endif

    char *last_slash = strrchr(buffer, '/');
    if(last_slash == nullptr) {
        return false;
    }

    *last_slash = 0;
    return true;
}

internal b32
build_shader_path(char *buffer, u64 buffer_size, char const *file_name) {
    assert(buffer != nullptr, "Shader path buffer must not be null!");
    assert(file_name != nullptr, "Shader file name must not be null!");

    char executable_directory[4096] = {};
    if(!get_executable_directory_for_renderer(
           executable_directory,
           sizeof(executable_directory)
       )) {
        return false;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "%s/shaders/%s",
        executable_directory,
        file_name
    );
    return written > 0 && (u64)written < buffer_size;
}

internal void *
read_binary_file(Arena *arena, char const *path, u64 *out_size) {
    assert(arena != nullptr, "Arena must not be null!");
    assert(path != nullptr, "File path must not be null!");
    assert(out_size != nullptr, "Size output must not be null!");

    FILE *file = fopen(path, "rb");
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

    void *data = push_size(arena, (u64)file_size);
    usize read_size = fread(data, 1, (usize)file_size, file);
    fclose(file);

    if(read_size != (usize)file_size) {
        return nullptr;
    }

    *out_size = (u64)file_size;
    return data;
}

internal b32
create_shader_module(char const *file_name, VkShaderModule *out_shader_module) {
    assert(file_name != nullptr, "Shader file name must not be null!");
    assert(
        out_shader_module != nullptr,
        "Shader module output must not be null!"
    );

    char shader_path[4096] = {};
    if(!build_shader_path(shader_path, sizeof(shader_path), file_name)) {
        LOG_ERROR("Failed to build shader path for %s.", file_name);
        return false;
    }

    TemporaryMemory temporary_memory = begin_temporary_memory(vk_state.arena);
    u64 shader_size = 0;
    void *shader_data =
        read_binary_file(vk_state.arena, shader_path, &shader_size);
    if(shader_data == nullptr || shader_size == 0) {
        LOG_ERROR("Failed to read shader %s.", shader_path);
        end_temporary_memory(temporary_memory);
        return false;
    }

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader_size;
    create_info.pCode = (u32 const *)shader_data;

    b32 result = vkCreateShaderModule(
                     vk_state.device,
                     &create_info,
                     nullptr,
                     out_shader_module
                 ) == VK_SUCCESS;
    end_temporary_memory(temporary_memory);
    return result;
}

internal void
cleanup_pipeline(void) {
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

internal b32
create_pipeline(void) {
    VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
    VkShaderModule fragment_shader_module = VK_NULL_HANDLE;
    b32 result = false;
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
