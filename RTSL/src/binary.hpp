#pragma once

// Small typed binary reader / writer for the RTSL artifact format.
//
// Replaces one append_u8/u16/u32/u64/string helper per size with a single
// templated write<T> / read<T> that dispatches on the type. Little-endian on
// the wire regardless of host endianness. Std-only, no lf::* dep.

#include "basic_types.hpp"

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

// Growing writer over a caller-owned byte vector. All integer writes are
// little-endian; strings are `u32 length` followed by their bytes.
class BinaryWriter {
  public:
	explicit BinaryWriter(std::vector<u8>& out) : out_(out) {}

	template <std::unsigned_integral T>
	void write(T value) {
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			out_.push_back(static_cast<u8>((value >> (i * 8)) & 0xffu));
		}
	}

	void write_string(std::string_view text) {
		write<u32>(static_cast<u32>(text.size()));
		out_.insert(out_.end(), text.begin(), text.end());
	}

	// Append `count` zero bytes — placeholder / alignment padding.
	void padding(std::size_t count) {
		out_.insert(out_.end(), count, u8{ 0 });
	}

	void reserve_extra(std::size_t bytes) {
		out_.reserve(out_.size() + bytes);
	}

	[[nodiscard]] std::size_t size() const { return out_.size(); }
	[[nodiscard]] std::vector<u8>& bytes() { return out_; }

  private:
	std::vector<u8>& out_;
};

// Cursored reader over a caller-owned byte span. Every read returns
// std::nullopt on truncation instead of throwing. Callers propagate via
// early-return.
class BinaryReader {
  public:
	explicit BinaryReader(std::span<const u8> data) : data_(data) {}

	template <std::unsigned_integral T>
	[[nodiscard]] std::optional<T> read() {
		if (cursor_ + sizeof(T) > data_.size())
			return std::nullopt;
		T value = 0;
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			value |= static_cast<T>(data_[cursor_ + i]) << (i * 8);
		}
		cursor_ += sizeof(T);
		return value;
	}

	// Peek an integer without advancing the cursor. Used by the container
	// header parser where a field decides which section follows.
	template <std::unsigned_integral T>
	[[nodiscard]] std::optional<T> peek(std::size_t offset = 0) const {
		if (cursor_ + offset + sizeof(T) > data_.size())
			return std::nullopt;
		T value = 0;
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			value |= static_cast<T>(data_[cursor_ + offset + i]) << (i * 8);
		}
		return value;
	}

	[[nodiscard]] std::optional<std::string> read_string() {
		const auto length = read<u32>();
		if (!length)
			return std::nullopt;
		if (cursor_ + *length > data_.size())
			return std::nullopt;
		std::string value(reinterpret_cast<const char*>(data_.data() + cursor_), *length);
		cursor_ += *length;
		return value;
	}

	// Skip `count` bytes; returns false on truncation.
	[[nodiscard]] bool skip(std::size_t count) {
		if (cursor_ + count > data_.size())
			return false;
		cursor_ += count;
		return true;
	}

	void seek(std::size_t position) { cursor_ = position; }

	[[nodiscard]] std::size_t cursor() const { return cursor_; }
	[[nodiscard]] std::size_t size() const { return data_.size(); }
	[[nodiscard]] std::size_t remaining() const { return data_.size() - cursor_; }
	[[nodiscard]] bool at_end() const { return cursor_ >= data_.size(); }
	[[nodiscard]] std::span<const u8> data() const { return data_; }

  private:
	std::span<const u8> data_;
	std::size_t cursor_ = 0;
};

} // namespace rtsl
