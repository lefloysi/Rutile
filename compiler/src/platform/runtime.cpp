#include "runtime.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <system_error>
#include <thread>

struct RuntimeMutex {
	std::mutex value;
};

struct RuntimeCondition {
	std::condition_variable value;
};

struct RuntimeThread {
	std::thread value;
};

struct RuntimeAtomic {
	std::atomic<std::uint64_t> value;
};

std::uint64_t runtime_clock_nanoseconds() {
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

RuntimeMutex* runtime_mutex_create() {
	return new (std::nothrow) RuntimeMutex;
}

void runtime_mutex_destroy(RuntimeMutex* mutex) {
	delete mutex;
}

void runtime_mutex_lock(RuntimeMutex* mutex) {
	mutex->value.lock();
}

void runtime_mutex_unlock(RuntimeMutex* mutex) {
	mutex->value.unlock();
}

RuntimeCondition* runtime_condition_create() {
	return new (std::nothrow) RuntimeCondition;
}

void runtime_condition_destroy(RuntimeCondition* condition) {
	delete condition;
}

int runtime_condition_wait(RuntimeCondition* condition, RuntimeMutex* mutex, std::uint32_t milliseconds) {
	std::unique_lock lock(mutex->value, std::adopt_lock);
	const auto result = condition->value.wait_for(lock, std::chrono::milliseconds(milliseconds));
	lock.release();
	return result == std::cv_status::no_timeout ? 1 : 0;
}

void runtime_condition_notify(RuntimeCondition* condition) {
	condition->value.notify_all();
}

RuntimeThread* runtime_thread_start(int (*callback)(void*), void* context) {
	if (!callback) {
		return nullptr;
	}
	try {
		auto thread = new RuntimeThread;
		try {
			thread->value = std::thread([callback, context]() {
				callback(context);
			});
		}
		catch (...) {
			delete thread;
			throw;
		}
		return thread;
	}
	catch (const std::bad_alloc&) {
		return nullptr;
	}
	catch (const std::system_error&) {
		return nullptr;
	}
}

void runtime_thread_join(RuntimeThread* thread) {
	if (!thread) {
		return;
	}
	if (thread->value.joinable()) {
		thread->value.join();
	}
	delete thread;
}

RuntimeAtomic* runtime_atomic_create(std::uint64_t value) {
	return new (std::nothrow) RuntimeAtomic{ value };
}

void runtime_atomic_destroy(RuntimeAtomic* value) {
	delete value;
}

std::uint64_t runtime_atomic_load(const RuntimeAtomic* value) {
	return value->value.load(std::memory_order_acquire);
}

void runtime_atomic_store(RuntimeAtomic* destination, std::uint64_t value) {
	destination->value.store(value, std::memory_order_release);
}

std::uint64_t runtime_atomic_fetch_add(RuntimeAtomic* destination, std::uint64_t value) {
	return destination->value.fetch_add(value, std::memory_order_acq_rel);
}
