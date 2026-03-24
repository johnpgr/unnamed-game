#include "base/base_threads.h"

bool init_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    return pthread_mutex_init(&mutex->handle, nullptr) == 0;
}

void destroy_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    pthread_mutex_destroy(&mutex->handle);
}

void lock_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    pthread_mutex_lock(&mutex->handle);
}

void unlock_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    pthread_mutex_unlock(&mutex->handle);
}

bool init_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    return pthread_cond_init(&condition_variable->handle, nullptr) == 0;
}

void destroy_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    pthread_cond_destroy(&condition_variable->handle);
}

void wake_all_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    pthread_cond_broadcast(&condition_variable->handle);
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
    pthread_cond_wait(&condition_variable->handle, &mutex->handle);
}

bool create_thread(Thread* thread, ThreadProc* proc, void* data) {
    ASSERT(thread != nullptr, "Thread must not be null!");
    ASSERT(proc != nullptr, "Thread proc must not be null!");
    return pthread_create(&thread->handle, nullptr, proc, data) == 0;
}

void join_thread(Thread* thread) {
    ASSERT(thread != nullptr, "Thread must not be null!");
    pthread_join(thread->handle, nullptr);
}

u32 get_logical_processor_count(void) {
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if(cpu_count < 1) {
        cpu_count = 1;
    }

    return (u32)cpu_count;
}
