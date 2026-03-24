#include "os/os_mod.h"

#if OS_WINDOWS
#include "os/os_threads_win32.cpp"
#else
#include "os/os_threads_posix.cpp"
#endif
