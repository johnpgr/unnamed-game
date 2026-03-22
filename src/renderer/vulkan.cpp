#include "renderer/vulkan.h"

global_variable VulkanState vk_state = {};

#include "renderer/vulkan_pipeline.cpp"
#include "renderer/vulkan_swapchain.cpp"
#include "renderer/vulkan_commands.cpp"
#include "renderer/vulkan_init.cpp"
