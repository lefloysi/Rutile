#include "declaration_filters.hpp"

#include "platform.hpp"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

using NativeFilterProcedure = int (*)(void*, const NativeDeclarationAttribute*, std::size_t, const char*, std::size_t, const char*, std::size_t, const char*, std::size_t, const NativeDeclarationAttribute*, std::size_t);

struct NativeLoadedFilter {
	void* library = nullptr;
	void* predicate = nullptr;
	NativeFilterProcedure apply = nullptr;

	~NativeLoadedFilter() {
		if (library) {
			platform_library_close(library);
		}
	}
};

struct NativeDeclarationFilters {
	std::vector<std::unique_ptr<NativeLoadedFilter>> filters;
	std::string root;
	std::string os;
	std::string profile;
	std::map<std::string, std::string> choices;
	std::vector<NativeDeclarationAttribute> choice_views;
};

thread_local std::string native_declaration_filter_error;
std::mutex native_declaration_filter_mutex;

NativeDeclarationFilters* declaration_filters_open(const char* json, std::size_t size) {
	try {
		native_declaration_filter_error.clear();
		const auto descriptor = nlohmann::json::parse(json, json + size);
		const auto context = descriptor.value("filter_context", nlohmann::json::object());
		auto result = std::make_unique<NativeDeclarationFilters>();
		result->root = context.value("root", "");
		result->os = context.value("os", "");
		result->profile = context.value("profile", "");
		result->choices = context.value("choices", std::map<std::string, std::string>{});
		for (const auto& [key, value] : result->choices) {
			result->choice_views.push_back({ key.data(), key.size(), value.data(), value.size() });
		}
		for (const auto& specification : descriptor.value("filters", nlohmann::json::array())) {
			auto filter = std::make_unique<NativeLoadedFilter>();
			const auto path = specification.at("module").get<std::string>();
			filter->library = platform_library_open(std::filesystem::u8path(path));
			if (!filter->library) {
				throw std::runtime_error("Could not load declaration filter module: " + path);
			}
			const auto anchor = reinterpret_cast<std::uintptr_t>(platform_library_symbol(filter->library, "kiln_configure"));
			filter->apply = reinterpret_cast<NativeFilterProcedure>(platform_library_symbol(filter->library, "kiln_filter"));
			if (!anchor || !filter->apply) {
				throw std::runtime_error("Declaration filter module has an incompatible interface: " + path);
			}
			const auto offset = specification.at("offset").get<std::int64_t>();
			filter->predicate = reinterpret_cast<void*>(anchor + static_cast<std::uintptr_t>(offset));
			result->filters.push_back(std::move(filter));
		}
		return result.release();
	}
	catch (const std::exception& error) {
		native_declaration_filter_error = error.what();
		return nullptr;
	}
}

void declaration_filters_close(NativeDeclarationFilters* filters) {
	delete filters;
}

int declaration_filters_apply(NativeDeclarationFilters* filters, const NativeDeclarationAttribute* attributes, std::size_t count) {
	if (!filters) {
		return 1;
	}
	std::lock_guard guard(native_declaration_filter_mutex);
	int result = 1;
	for (const auto& filter : filters->filters) {
		const int selected = filter->apply(filter->predicate, attributes, count, filters->root.data(), filters->root.size(), filters->os.data(), filters->os.size(), filters->profile.data(), filters->profile.size(), filters->choice_views.data(), filters->choice_views.size());
		if (selected < 0) {
			result = -1;
		}
		else if (!selected && result > 0) {
			result = 0;
		}
	}
	return result;
}

const char* declaration_filters_error() {
	return native_declaration_filter_error.c_str();
}
