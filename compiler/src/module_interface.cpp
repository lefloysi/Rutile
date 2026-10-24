#include "module_interface.hpp"

#include "version.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <vector>

extern "C" int rutile_decode_interface(const char* bytes, std::size_t size, const char* version, std::uint32_t pointer_bits, bool test_access, int (*append)(void*, const char*, const char*, std::size_t, const char*, int), void* context);
extern "C" int rutile_encode_interface(const char* module, const char* const* paths, const char* const* texts, const std::size_t* lengths, const char* const* owners, std::size_t count, const char* version, std::uint32_t pointer_bits, int (*write)(void*, const char*, std::size_t), void* context);

int rutile_interface_append(void* context, const char* path, const char* text, std::size_t size, const char* module, int access) {
	auto& records = *static_cast<nlohmann::json*>(context);
	try {
		records.push_back({ { "path", path }, { "text", std::string(text, size) }, { "module", module }, { "access", access } });
	}
	catch (const std::bad_alloc&) {
		return 0;
	}
	return 1;
}

nlohmann::json rutile_interface_sources(std::string_view bytes, bool test_access) {
	auto records = nlohmann::json::array();
	const auto result = rutile_decode_interface(bytes.data(), bytes.size(), software_version.data(), sizeof(void*) * 8, test_access, rutile_interface_append, &records);
	if (result == 2) {
		throw std::bad_alloc();
	}
	if (result) {
		throw std::runtime_error("Invalid or incompatible Rutile interface: compiler version and target must match.");
	}
	return records;
}

int rutile_interface_write(void* context, const char* bytes, std::size_t size) {
	try {
		static_cast<std::string*>(context)->assign(bytes, size);
	}
	catch (const std::bad_alloc&) {
		return 0;
	}
	return 1;
}

std::string rutile_interface_encode(std::string_view module, const nlohmann::json& records) {
	std::vector<const char*> paths;
	std::vector<const char*> texts;
	std::vector<std::size_t> lengths;
	std::vector<const char*> owners;
	for (const auto& record : records) {
		paths.push_back(record.at("path").get_ref<const std::string&>().c_str());
		const auto& text = record.at("text").get_ref<const std::string&>();
		texts.push_back(text.data());
		lengths.push_back(text.size());
		owners.push_back(record.at("module").get_ref<const std::string&>().c_str());
	}
	const std::string module_name(module);
	std::string bytes;
	const auto result = rutile_encode_interface(module_name.c_str(), paths.data(), texts.data(), lengths.data(), owners.data(), records.size(), software_version.data(), sizeof(void*) * 8, rutile_interface_write, &bytes);
	if (result == 2) {
		throw std::bad_alloc();
	}
	if (result) {
		throw std::runtime_error("Cannot encode Rutile interface.");
	}
	return bytes;
}
