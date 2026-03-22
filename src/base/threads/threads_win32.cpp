#include "base/threads/threads.h"

#if OS_WINDOWS

internal bool init_thread_mutex(ThreadMutex *mutex) {
    assert(mutex != nullptr, "Thread mutex must not be null!");
    InitializeCriticalSection(&mutex->handle);
    return true;
}

internal void destroy_thread_mutex(ThreadMutex *mutex) {
    assert(mutex != nullptr, "Thread mutex must not be null!");
    DeleteCriticalSection(&mutex->handle);
}

internal void lock_thread_mutex(ThreadMutex *mutex) {
    assert(mutex != nullptr, "Thread mutex must not be null!");
    EnterCriticalSection(&mutex->handle);
}

internal void unlock_thread_mutex(ThreadMutex *mutex) {
    assert(mutex != nullptr, "Thread mutex must not be null!");
    LeaveCriticalSection(&mutex->handle);
}

internal bool init_thread_condition_variable(
    ThreadConditionVariable *condition_variable
) {
    assert(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    InitializeConditionVariable(&condition_variable->handle);
    return true;
}

internal void destroy_thread_condition_variable(
    ThreadConditionVariable *condition_variable
) {
    assert(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
}

internal void wake_all_thread_condition_variable(
    ThreadConditionVariable *condition_variable
) {
    assert(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    WakeAllConditionVariable(&condition_variable->handle);
}

internal void wait_thread_condition_variable(
    ThreadConditionVariable *condition_variable,
    ThreadMutex *mutex
) {
    assert(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    assert(mutex != nullptr, "Thread mutex must not be null!");
    SleepConditionVariableCS(
        &condition_variable->handle,
        &mutex->handle,
        INFINITE
    );
}

internal bool create_thread(Thread *thread, ThreadProc *proc, void *data) {
    assert(thread != nullptr, "Thread must not be null!");
    assert(proc != nullptr, "Thread proc must not be null!");

    thread->handle = CreateThread(nullptr, 0, proc, data, 0, nullptr);
    return thread->handle != nullptr;
}

internal void join_thread(Thread *thread) {
    assert(thread != nullptr, "Thread must not be null!");
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    thread->handle = nullptr;
}

internal u32 get_logical_processor_count(void) {
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    if(system_info.dwNumberOfProcessors == 0) {
        return 1;
    }

    return (u32)system_info.dwNumberOfProcessors;
}

#endif
