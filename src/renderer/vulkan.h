#pragma once

#include "base/lane.h"
#include "game/game_math.h"
#include "game/game_render_group.h"

#include <vulkan/vulkan.h>

struct Arena;
struct GLFWwindow;

#define MAX_SWAPCHAIN_IMAGES 4

struct VulkanSpritePushConstants {
    vec2 center;
    vec2 size;
    vec4 color;
    vec2 screen_size;
};

struct VulkanState {
    Arena *arena;
    GLFWwindow *window;

    VkInstance instance;
    VkApplicationInfo app_info;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkSurfaceKHR surface;
    u32 graphics_queue_family_index;
    VkDebugUtilsMessengerEXT debug_messenger;
    b32 dynamic_rendering_supported;
    b32 initialized;
    volatile b32 fatal_error;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    u32 swapchain_image_count;
    VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchain_views[MAX_SWAPCHAIN_IMAGES];

    VkCommandPool lane_pools[MAX_LANES];
    VkCommandBuffer lane_cmds[MAX_LANES];
    u32 active_lane_count;

    VkCommandPool primary_pool;
    VkCommandBuffer primary_cmd;

    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence frame_fence;

    VkPipelineLayout pipeline_layout;
    VkPipeline sprite_pipeline;

    u32 frame_image_index;
    b32 frame_active;

    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering;
    PFN_vkCmdEndRenderingKHR cmd_end_rendering;
};

b32 init_vulkan(Arena *arena, GLFWwindow *window, u32 lane_count);
void cleanup_vulkan(void);
b32 begin_frame(void);
b32 render_group_to_output(RenderCommands *commands);
