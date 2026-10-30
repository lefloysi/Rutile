#include <rt/runtime.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>

namespace rt::runtime {
	int report_error(string_view description) noexcept {
		std::fwrite(description.data(), sizeof(char), description.size(), stderr);
		if (description.empty() || description.back() != '\n') {
			std::fputc('\n', stderr);
		}
		return EXIT_FAILURE;
	}

	int run(int count, char* arguments[], entry application) noexcept {
		try {
			std::vector<string_view> views;
			if (count > 1) {
				views.reserve(static_cast<std::size_t>(count - 1));
			}
			for (int index = 1; index < count; ++index) {
				views.emplace_back(arguments[index]);
			}
			auto result = application(views);
			if (!result) {
				return report_error(result.error().message);
			}
			return EXIT_SUCCESS;
		}
		catch (const std::exception& error) {
			return report_error(error.what());
		}
		catch (...) {
			return report_error("Unhandled non-standard exception.");
		}
	}
}
