#include "atomic.h"

#include <atomic>
#include <assert.h>

void rtvk_atomic_bool_store(bool* value, bool next) {
	assert(value);
	std::atomic_ref<bool>(*value).store(next, std::memory_order_relaxed);
}

bool rtvk_atomic_bool_load(const bool* value) {
	assert(value);
	return std::atomic_ref<const bool>(*value).load(std::memory_order_relaxed);
}

void rtvk_atomic_store(u32* value, u32 next) {
	assert(value);
	std::atomic_ref<u32>(*value).store(next, std::memory_order_relaxed);
}

u32 rtvk_atomic_load(const u32* value) {
	assert(value);
	return std::atomic_ref<const u32>(*value).load(std::memory_order_relaxed);
}

u32 rtvk_atomic_inc(u32* value) {
	assert(value);
	return std::atomic_ref<u32>(*value).fetch_add(1, std::memory_order_relaxed) + 1;
}

u32 rtvk_atomic_dec(u32* value) {
	assert(value);
	return std::atomic_ref<u32>(*value).fetch_sub(1, std::memory_order_relaxed) - 1;
}
