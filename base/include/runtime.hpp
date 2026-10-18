#pragma once

#include <cstdint>

struct RuntimeMutex;
struct RuntimeCondition;
struct RuntimeThread;
struct RuntimeAtomic;

extern "C" std::uint64_t runtime_clock_nanoseconds();

extern "C" RuntimeMutex* runtime_mutex_create();
extern "C" void runtime_mutex_destroy(RuntimeMutex* mutex);
extern "C" void runtime_mutex_lock(RuntimeMutex* mutex);
extern "C" void runtime_mutex_unlock(RuntimeMutex* mutex);
extern "C" RuntimeCondition* runtime_condition_create();
extern "C" void runtime_condition_destroy(RuntimeCondition* condition);
// The caller owns the mutex on entry and on return, including a timed-out wait.
extern "C" int runtime_condition_wait(RuntimeCondition* condition, RuntimeMutex* mutex, std::uint32_t milliseconds);
extern "C" void runtime_condition_notify(RuntimeCondition* condition);
extern "C" RuntimeThread* runtime_thread_start(int (*callback)(void*), void* context);
extern "C" void runtime_thread_join(RuntimeThread* thread);
extern "C" RuntimeAtomic* runtime_atomic_create(std::uint64_t value);
extern "C" void runtime_atomic_destroy(RuntimeAtomic* value);
extern "C" std::uint64_t runtime_atomic_load(const RuntimeAtomic* value);
extern "C" void runtime_atomic_store(RuntimeAtomic* destination, std::uint64_t value);
extern "C" std::uint64_t runtime_atomic_fetch_add(RuntimeAtomic* destination, std::uint64_t value);
