#pragma once

#include "types.hpp"

#include <atomic>

using atomic_bool = std::atomic<bool>;
using atomic_u32 = std::atomic<u32>;

inline void rtdx_atomic_bool_init(atomic_bool* value, bool initial) { value->store(initial, std::memory_order_relaxed); }
inline void rtdx_atomic_bool_finish(atomic_bool*) {}
inline void rtdx_atomic_bool_store(atomic_bool* value, bool next) { value->store(next, std::memory_order_relaxed); }
inline bool rtdx_atomic_bool_load(const atomic_bool* value) { return value->load(std::memory_order_relaxed); }

inline void rtdx_atomic_u32_init(atomic_u32* value, u32 initial) { value->store(initial, std::memory_order_relaxed); }
inline void rtdx_atomic_u32_finish(atomic_u32*) {}
inline void rtdx_atomic_store(atomic_u32* value, u32 next) { value->store(next, std::memory_order_relaxed); }
inline u32 rtdx_atomic_load(const atomic_u32* value) { return value->load(std::memory_order_relaxed); }
inline u32 rtdx_atomic_inc(atomic_u32* value) { return value->fetch_add(1, std::memory_order_relaxed) + 1; }
inline u32 rtdx_atomic_dec(atomic_u32* value) { return value->fetch_sub(1, std::memory_order_relaxed) - 1; }
