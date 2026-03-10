#pragma once

ArrayList<const char*> pvkGetInstanceExtensions(Arena* arena);

bool pvkCreateSurface(
    VkInstance instance,
    VkSurfaceKHR* out_surface
);
