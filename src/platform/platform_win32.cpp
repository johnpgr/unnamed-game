#include "base/defines.h"

#if OS_WINDOWS

#define WIN32_LEAN_AND_MEAN

#include "platform/window/window_win32.cpp"
#include "platform/vulkan/vulkan_win32.cpp"
#include "platform/audio/audio_win32.cpp"
#include "platform/input/input_win32.cpp"
#include "platform/dynlib/dynlib_win32.cpp"
#include "platform/filesystem/filesystem_win32.cpp"

#endif
