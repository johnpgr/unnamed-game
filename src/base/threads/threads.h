#pragma once

#include "base/core.h"

#if OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

typedef DWORD ThreadProcResult;
#define THREAD_PROC_CALL WINAPI
#define THREAD_PROC_SUCCESS 0

struct Thread {
    HANDLE handle;
};

struct ThreadMutex {
    CRITICAL_SECTION handle;
};

struct ThreadConditionVariable {
    CONDITION_VARIABLE handle;
};
#else
#include <pthread.h>
#include <unistd.h>

typedef void *ThreadProcResult;
#define THREAD_PROC_CALL
#define THREAD_PROC_SUCCESS nullptr

struct Thread {
    pthread_t handle;
};

struct ThreadMutex {
    pthread_mutex_t handle;
};

struct ThreadConditionVariable {
    pthread_cond_t handle;
};
#endif

typedef ThreadProcResult THREAD_PROC_CALL ThreadProc(void *data);

internal bool init_thread_mutex(ThreadMutex *mutex);
internal void destroy_thread_mutex(ThreadMutex *mutex);
internal void lock_thread_mutex(ThreadMutex *mutex);
internal void unlock_thread_mutex(ThreadMutex *mutex);

internal bool init_thread_condition_variable(
    ThreadConditionVariable *condition_variable
);
internal void destroy_thread_condition_variable(
    ThreadConditionVariable *condition_variable
);
internal void wake_all_thread_condition_variable(
    ThreadConditionVariable *condition_variable
);
internal void wait_thread_condition_variable(
    ThreadConditionVariable *condition_variable,
    ThreadMutex *mutex
);

internal bool create_thread(Thread *thread, ThreadProc *proc, void *data);
internal void join_thread(Thread *thread);

internal u32 get_logical_processor_count(void);
