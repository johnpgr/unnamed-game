#include "base/base_threads.h"

#if OS_WINDOWS

bool init_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    InitializeCriticalSection(&mutex->handle);
    return true;
}

void destroy_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    DeleteCriticalSection(&mutex->handle);
}

void lock_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    EnterCriticalSection(&mutex->handle);
}

void unlock_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    LeaveCriticalSection(&mutex->handle);
}

bool init_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    InitializeConditionVariable(&condition_variable->handle);
    return true;
}

void destroy_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
}

void wake_all_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    WakeAllConditionVariable(&condition_variable->handle);
}

void wait_thread_condition_variable(
    ThreadConditionVariable* condition_variable,
    ThreadMutex* mutex
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    SleepConditionVariableCS(
        &condition_variable->handle,
        &mutex->handle,
        INFINITE
    );
}

bool create_thread(Thread* thread, ThreadProc* proc, void* data) {
    ASSERT(thread != nullptr, "Thread must not be null!");
    ASSERT(proc != nullptr, "Thread proc must not be null!");

    thread->handle = CreateThread(nullptr, 0, proc, data, 0, nullptr);
    return thread->handle != nullptr;
}

void join_thread(Thread* thread) {
    ASSERT(thread != nullptr, "Thread must not be null!");
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    thread->handle = nullptr;
}

u32 get_logical_processor_count(void) {
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    if(system_info.dwNumberOfProcessors == 0) {
        return 1;
    }

    return (u32)system_info.dwNumberOfProcessors;
}

#endif
