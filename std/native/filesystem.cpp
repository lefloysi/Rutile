#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <string>
#include <system_error>

enum class native_file_error_kind : int {
	success,
	not_found,
	permission_denied,
	io,
	allocation,
};

struct NativeFileResult {
	char* data;
	std::size_t size;
	native_file_error_kind error_kind;
	int native_code;
};

void file_error(NativeFileResult& result, std::error_code error) {
	result.error_kind = native_file_error_kind::io;
	result.native_code = error.value();
	if (error == std::errc::no_such_file_or_directory) {
		result.error_kind = native_file_error_kind::not_found;
	}
	else if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) {
		result.error_kind = native_file_error_kind::permission_denied;
	}
	else if (error == std::errc::not_enough_memory) {
		result.error_kind = native_file_error_kind::allocation;
	}
}

void file_copy(NativeFileResult& result, const char* data, std::size_t size) {
	result.size = size;
	result.data = static_cast<char*>(std::malloc(size ? size : 1));
	if (!result.data) {
		file_error(result, std::make_error_code(std::errc::not_enough_memory));
		return;
	}
	std::memcpy(result.data, data, size);
	result.size = size;
}

extern "C" void rutile_file_read(const char* path, std::size_t path_size, NativeFileResult* result) {
	*result = {};
	if (std::memchr(path, 0, path_size)) {
		file_error(*result, std::make_error_code(std::errc::invalid_argument));
		return;
	}
	try {
		errno = 0;
		std::ifstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
		if (!file) {
			file_error(*result, std::error_code(errno ? errno : EIO, std::generic_category()));
			return;
		}
		auto length = file.tellg();
		if (length < 0 || static_cast<std::uintmax_t>(length) > (std::numeric_limits<std::size_t>::max)()) {
			file_error(*result, std::make_error_code(std::errc::file_too_large));
			return;
		}
		std::size_t size = static_cast<std::size_t>(length);
		result->size = size;
		result->data = static_cast<char*>(std::malloc(size ? size : 1));
		if (!result->data) {
			file_error(*result, std::make_error_code(std::errc::not_enough_memory));
			return;
		}
		file.seekg(0);
		file.read(result->data, static_cast<std::streamsize>(size));
		if (!file) {
			std::free(result->data);
			result->data = nullptr;
			file_error(*result, std::make_error_code(std::errc::io_error));
			return;
		}
		result->size = size;
	}
	catch (const std::bad_alloc&) {
		file_error(*result, std::make_error_code(std::errc::not_enough_memory));
	}
	catch (const std::filesystem::filesystem_error& error) {
		file_error(*result, error.code());
	}
}

extern "C" void rutile_file_write(const char* path, std::size_t path_size, const char* bytes, std::size_t size, NativeFileResult* result) {
	*result = {};
	if (std::memchr(path, 0, path_size)) {
		file_error(*result, std::make_error_code(std::errc::invalid_argument));
		return;
	}
	if (size > static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)())) {
		file_error(*result, std::make_error_code(std::errc::file_too_large));
		return;
	}
	try {
		errno = 0;
		std::ofstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
		if (!file) {
			file_error(*result, std::error_code(errno ? errno : EIO, std::generic_category()));
			return;
		}
		file.write(bytes, static_cast<std::streamsize>(size));
		file.close();
		if (!file) {
			file_error(*result, std::make_error_code(std::errc::io_error));
		}
	}
	catch (const std::bad_alloc&) {
		file_error(*result, std::make_error_code(std::errc::not_enough_memory));
	}
	catch (const std::filesystem::filesystem_error& error) {
		file_error(*result, error.code());
	}
}

extern "C" void rutile_create_directories(const char* path, std::size_t path_size, NativeFileResult* result) {
	*result = {};
	if (std::memchr(path, 0, path_size)) {
		file_error(*result, std::make_error_code(std::errc::invalid_argument));
		return;
	}
	try {
		std::error_code error;
		std::filesystem::create_directories(std::filesystem::u8path(path), error);
		if (error) {
			file_error(*result, error);
		}
	}
	catch (const std::bad_alloc&) {
		file_error(*result, std::make_error_code(std::errc::not_enough_memory));
	}
}

extern "C" void rutile_temporary_directory(NativeFileResult* result) {
	*result = {};
	try {
		std::error_code error;
		auto path = std::filesystem::temp_directory_path(error);
		if (error) {
			file_error(*result, error);
			return;
		}
		auto bytes = path.u8string();
		file_copy(*result, reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}
	catch (const std::bad_alloc&) {
		file_error(*result, std::make_error_code(std::errc::not_enough_memory));
	}
}

extern "C" void rutile_environment(const char* name, NativeFileResult* result) {
	*result = {};
	if (const char* value = std::getenv(name)) {
		file_copy(*result, value, std::strlen(value));
	}
}
