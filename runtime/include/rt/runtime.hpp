#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace rt {
	using string_view = std::string_view;

	using std::span;

	namespace runtime {
		struct error {
			std::string message;
		};

		using result = std::expected<void, error>;
		using entry = result (*)(span<string_view> arguments);

		int run(int count, char* arguments[], entry application) noexcept;
	}
}
