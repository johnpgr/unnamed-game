#include "renderer/vulkan.h"

#include "base/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

internal b32
create_command_buffers(u32 lane_count) {
    assert(lane_count > 0, "Lane count must be non-zero!");
    assert(lane_count <= MAX_LANES, "Lane count exceeds MAX_LANES!");

    vk_state.active_lane_count = lane_count;

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

    for(u32 lane_index = 0; lane_index < lane_count; ++lane_index) {
        if(vkCreateCommandPool(
               vk_state.device,
               &pool_info,
               nullptr,
               &vk_state.lane_pools[lane_index]
           ) != VK_SUCCESS) {
            return false;
        }

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = vk_state.lane_pools[lane_index];
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        alloc_info.commandBufferCount = 1;

        if(vkAllocateCommandBuffers(
               vk_state.device,
               &alloc_info,
               &vk_state.lane_cmds[lane_index]
           ) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

internal void
cleanup_command_buffers(void) {
    for(u32 lane_index = 0; lane_index < ARRAY_COUNT(vk_state.lane_pools);
        ++lane_index) {
        if(vk_state.lane_pools[lane_index] != VK_NULL_HANDLE) {
            vkDestroyCommandPool(
                vk_state.device,
                vk_state.lane_pools[lane_index],
                nullptr
            );
            vk_state.lane_pools[lane_index] = VK_NULL_HANDLE;
            vk_state.lane_cmds[lane_index] = VK_NULL_HANDLE;
        }
    }

    if(vk_state.primary_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vk_state.device, vk_state.primary_pool, nullptr);
        vk_state.primary_pool = VK_NULL_HANDLE;
        vk_state.primary_cmd = VK_NULL_HANDLE;
    }
}

internal vec4
get_clear_color(RenderCommands *commands) {
    vec4 result = vec4(0.04f, 0.05f, 0.08f, 1.0f);

    for(u32 lane_index = 0; lane_index < commands->active_lane_count;
        ++lane_index) {
        LanePushBuffer *buffer = commands->lane_buffers + lane_index;
        for(u32 offset = 0; offset < buffer->used;) {
            RenderEntryHeader *header =
                (RenderEntryHeader *)(buffer->base + offset);
            if(header->type == render_entry_type_clear) {
                RenderEntryClear *entry = (RenderEntryClear *)header;
                result = entry->color;
                return result;
            }

            offset += header->size;
        }
    }

    return result;
}

b32
begin_frame(void) {
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

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(
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

internal b32
vulkan_record_commands(RenderCommands *commands) {
    assert(commands != nullptr, "Render commands must not be null!");

    if(!vk_state.frame_active || vk_state.fatal_error) {
        return !vk_state.fatal_error;
    }

    u32 lane_index = lane_idx();
    assert(lane_index < vk_state.active_lane_count, "Lane index out of range!");

    if(vkResetCommandPool(
           vk_state.device,
           vk_state.lane_pools[lane_index],
           0
       ) != VK_SUCCESS) {
        LOG_ERROR("vkResetCommandPool failed for lane %u.", lane_index);
        return false;
    }

    VkCommandBufferInheritanceRenderingInfoKHR rendering_info = {};
    rendering_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &vk_state.swapchain_format;
    rendering_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkCommandBufferInheritanceInfo inheritance_info = {};
    inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance_info.pNext = &rendering_info;

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                       VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    begin_info.pInheritanceInfo = &inheritance_info;

    if(vkBeginCommandBuffer(vk_state.lane_cmds[lane_index], &begin_info) !=
       VK_SUCCESS) {
        LOG_ERROR("vkBeginCommandBuffer failed for lane %u.", lane_index);
        return false;
    }

    vkCmdBindPipeline(
        vk_state.lane_cmds[lane_index],
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
    vkCmdSetViewport(vk_state.lane_cmds[lane_index], 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = vk_state.swapchain_extent;
    vkCmdSetScissor(vk_state.lane_cmds[lane_index], 0, 1, &scissor);

    LanePushBuffer *buffer = commands->lane_buffers + lane_index;
    for(u32 offset = 0; offset < buffer->used;) {
        RenderEntryHeader *header =
            (RenderEntryHeader *)(buffer->base + offset);
        if(header->type == render_entry_type_rect) {
            RenderEntryRect *entry = (RenderEntryRect *)header;
            VulkanSpritePushConstants push_constants = {};
            push_constants.center = entry->p;
            push_constants.size = vec2(entry->width, entry->height);
            push_constants.color = entry->color;
            push_constants.screen_size =
                vec2((f32)commands->screen_width, (f32)commands->screen_height);

            vkCmdPushConstants(
                vk_state.lane_cmds[lane_index],
                vk_state.pipeline_layout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(push_constants),
                &push_constants
            );
            vkCmdDraw(vk_state.lane_cmds[lane_index], 6, 1, 0, 0);
        }

        offset += header->size;
    }

    if(vkEndCommandBuffer(vk_state.lane_cmds[lane_index]) != VK_SUCCESS) {
        LOG_ERROR("vkEndCommandBuffer failed for lane %u.", lane_index);
        return false;
    }

    return true;
}

internal void
transition_swapchain_image(
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

internal b32
end_frame(RenderCommands *commands) {
    assert(commands != nullptr, "Render commands must not be null!");

    if(!vk_state.frame_active || vk_state.fatal_error) {
        return !vk_state.fatal_error;
    }

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

    vec4 clear_color = get_clear_color(commands);
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
    rendering_info.flags =
        VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR;
    rendering_info.renderArea.extent = vk_state.swapchain_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    vk_state.cmd_begin_rendering(vk_state.primary_cmd, &rendering_info);
    vkCmdExecuteCommands(
        vk_state.primary_cmd,
        vk_state.active_lane_count,
        vk_state.lane_cmds
    );
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
    submit_info.pSignalSemaphores = &vk_state.render_finished_semaphore;

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
    present_info.pWaitSemaphores = &vk_state.render_finished_semaphore;
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

b32
render_group_to_output(RenderCommands *commands) {
    assert(commands != nullptr, "Render commands must not be null!");

    b32 result = true;
    if(!vk_state.fatal_error) {
        result = vulkan_record_commands(commands);
        if(!result) {
            vk_state.fatal_error = true;
        }
    }

    lane_sync();

    if(lane_idx() == 0 && !vk_state.fatal_error) {
        result = end_frame(commands);
        if(!result) {
            vk_state.fatal_error = true;
        }
    }

    lane_sync();
    return !vk_state.fatal_error;
}
