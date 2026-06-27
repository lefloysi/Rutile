#ifndef RUTILE_EXAMPLE_UTIL_H
#define RUTILE_EXAMPLE_UTIL_H

#include <cstddef>
#include <span>

template <typename T>
std::span<const std::byte> to_bytes(const T &value) {
	return {
		reinterpret_cast<const std::byte *>(&value),
		sizeof(T),
	};
}

template <typename T>
std::span<const std::byte> to_bytes(std::span<const T> values) {
	return {
		reinterpret_cast<const std::byte *>(values.data()),
		values.size_bytes(),
	};
}

template <typename T>
std::span<std::byte> to_bytes(std::span<T> values) {
	return {
		reinterpret_cast<std::byte *>(values.data()),
		values.size_bytes(),
	};
}

#endif
