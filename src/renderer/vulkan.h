#pragma once

struct Arena;
struct SDL_Window;

bool InitVulkan(Arena* arena, SDL_Window* window);
void CleanupVulkan(void);
bool Draw(void);
