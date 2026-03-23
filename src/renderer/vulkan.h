#pragma once

#include "base/types.h"
#include "push_cmds.cpp"

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

bool init_vulkan(Arena *arena, GLFWwindow *window);
void cleanup_vulkan(void);
bool begin_frame(void);
bool render_drain_cmd_buffer(PushCmdBuffer *buffer);
