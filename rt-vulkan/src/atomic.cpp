#include "atomic.h"

#include <atomic>
#include <new>

static_assert(sizeof(std::atomic<u32>) == sizeof(((atomic_u32*)0)->storage), "atomic_u32 storage size does not match std::atomic<u32>");
static_assert(alignof(std::atomic<u32>) <= alignof(atomic_u32), "atomic_u32 storage alignment does not match std::atomic<u32>");
static_assert(sizeof(std::atomic<bool>) == sizeof(((atomic_bool*)0)->storage), "atomic_bool storage size does not match std::atomic<bool>");
static_assert(alignof(std::atomic<bool>) <= alignof(atomic_bool), "atomic_bool storage alignment does not match std::atomic<bool>");

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static std::atomic<u32>* rtvk_atomic_ptr(atomic_u32* value) {
	return reinterpret_cast<std::atomic<u32>*>(value->storage);
}

static const std::atomic<u32>* rtvk_atomic_ptr(const atomic_u32* value) {
	return reinterpret_cast<const std::atomic<u32>*>(value->storage);
}

static std::atomic<bool>* rtvk_atomic_bool_ptr(atomic_bool* value) {
	return reinterpret_cast<std::atomic<bool>*>(value->storage);
}

static const std::atomic<bool>* rtvk_atomic_bool_ptr(const atomic_bool* value) {
	return reinterpret_cast<const std::atomic<bool>*>(value->storage);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvk_atomic_bool_init(atomic_bool* value, bool initial) {
	new (value->storage) std::atomic<bool>(initial);
}

void rtvk_atomic_bool_finish(atomic_bool* value) {
	rtvk_atomic_bool_ptr(value)->~atomic();
}

void rtvk_atomic_bool_store(atomic_bool* value, bool next) {
	rtvk_atomic_bool_ptr(value)->store(next, std::memory_order_relaxed);
}

bool rtvk_atomic_bool_load(const atomic_bool* value) {
	return rtvk_atomic_bool_ptr(value)->load(std::memory_order_relaxed);
}

void rtvk_atomic_u32_init(atomic_u32* value, u32 initial) {
	new (value->storage) std::atomic<u32>(initial);
}

void rtvk_atomic_u32_finish(atomic_u32* value) {
	rtvk_atomic_ptr(value)->~atomic();
}

void rtvk_atomic_store(atomic_u32* value, u32 next) {
	rtvk_atomic_ptr(value)->store(next, std::memory_order_relaxed);
}

u32 rtvk_atomic_load(const atomic_u32* value) {
	return rtvk_atomic_ptr(value)->load(std::memory_order_relaxed);
}

u32 rtvk_atomic_inc(atomic_u32* value) {
	return rtvk_atomic_ptr(value)->fetch_add(1, std::memory_order_relaxed) + 1;
}

u32 rtvk_atomic_dec(atomic_u32* value) {
	return rtvk_atomic_ptr(value)->fetch_sub(1, std::memory_order_relaxed) - 1;
}


