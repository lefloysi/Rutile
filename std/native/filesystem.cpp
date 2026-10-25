#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <string>
#include <stdexcept>
#include <system_error>

extern "C" {
void rutile_fs_text_complete(void* result, const char* bytes, std::size_t size);
void rutile_fs_text_not_found(void* result);
void rutile_fs_text_permission_denied(void* result);
void rutile_fs_text_io_error(void* result);
void rutile_fs_text_allocation_error(void* result, std::size_t bytes);
void rutile_fs_operation_complete(void* result);
void rutile_fs_operation_not_found(void* result);
void rutile_fs_operation_permission_denied(void* result);
void rutile_fs_operation_io_error(void* result);
void rutile_fs_operation_allocation_error(void* result, std::size_t bytes);
}

void file_read_error(void* result, std::error_code error, std::size_t allocation_size = 0) {
	if (error == std::errc::no_such_file_or_directory) {
		rutile_fs_text_not_found(result);
	}
	else if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) {
		rutile_fs_text_permission_denied(result);
	}
	else if (error == std::errc::not_enough_memory) {
		rutile_fs_text_allocation_error(result, allocation_size);
	}
	else {
		rutile_fs_text_io_error(result);
	}
}

void file_operation_error(void* result, std::error_code error) {
	if (error == std::errc::no_such_file_or_directory) {
		rutile_fs_operation_not_found(result);
	}
	else if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) {
		rutile_fs_operation_permission_denied(result);
	}
	else if (error == std::errc::not_enough_memory) {
		rutile_fs_operation_allocation_error(result, 0);
	}
	else {
		rutile_fs_operation_io_error(result);
	}
}

extern "C" void rutile_file_read(const char* path, std::size_t path_size, void* result) {
	if (!path_size || std::memchr(path, 0, path_size)) {
		rutile_fs_text_io_error(result);
		return;
	}
	std::size_t size = 0;
	try {
		errno = 0;
		std::ifstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
		if (!file) {
			file_read_error(result, std::error_code(errno ? errno : EIO, std::generic_category()));
			return;
		}
		auto length = file.tellg();
		if (length < 0 || static_cast<std::uintmax_t>(length) > (std::numeric_limits<std::size_t>::max)()
			|| static_cast<std::uintmax_t>(length) > static_cast<std::uintmax_t>((std::numeric_limits<std::streamsize>::max)())) {
			rutile_fs_text_io_error(result);
			return;
		}
		size = static_cast<std::size_t>(length);
		std::string bytes(size, char{});
		file.seekg(0);
		file.read(bytes.data(), static_cast<std::streamsize>(size));
		if (!file) {
			rutile_fs_text_io_error(result);
			return;
		}
		rutile_fs_text_complete(result, bytes.data(), bytes.size());
	}
	catch (const std::bad_alloc&) {
		rutile_fs_text_allocation_error(result, size);
	}
	catch (const std::filesystem::filesystem_error& error) {
		file_read_error(result, error.code(), size);
	}
	catch (const std::length_error&) {
		rutile_fs_text_io_error(result);
	}
}

extern "C" void rutile_file_write(const char* path, std::size_t path_size, const char* bytes, std::size_t size, void* result) {
	if (!path_size || std::memchr(path, 0, path_size) || size > static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)())) {
		rutile_fs_operation_io_error(result);
		return;
	}
	try {
		errno = 0;
		std::ofstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
		if (!file) {
			file_operation_error(result, std::error_code(errno ? errno : EIO, std::generic_category()));
			return;
		}
		file.write(bytes, static_cast<std::streamsize>(size));
		file.close();
		if (!file) {
			rutile_fs_operation_io_error(result);
			return;
		}
		rutile_fs_operation_complete(result);
	}
	catch (const std::bad_alloc&) {
		rutile_fs_operation_allocation_error(result, 0);
	}
	catch (const std::filesystem::filesystem_error& error) {
		file_operation_error(result, error.code());
	}
	catch (const std::length_error&) {
		rutile_fs_operation_io_error(result);
	}
}

extern "C" void rutile_create_directories(const char* path, std::size_t path_size, void* result) {
	if (!path_size || std::memchr(path, 0, path_size)) {
		rutile_fs_operation_io_error(result);
		return;
	}
	try {
		std::error_code error;
		std::filesystem::create_directories(std::filesystem::u8path(path), error);
		if (error) {
			file_operation_error(result, error);
			return;
		}
		rutile_fs_operation_complete(result);
	}
	catch (const std::bad_alloc&) {
		rutile_fs_operation_allocation_error(result, 0);
	}
	catch (const std::filesystem::filesystem_error& error) {
		file_operation_error(result, error.code());
	}
	catch (const std::length_error&) {
		rutile_fs_operation_io_error(result);
	}
}

extern "C" void rutile_temporary_directory(void* result) {
	try {
		std::error_code error;
		auto path = std::filesystem::temp_directory_path(error);
		if (error) {
			file_read_error(result, error);
			return;
		}
		auto bytes = path.u8string();
		rutile_fs_text_complete(result, reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}
	catch (const std::bad_alloc&) {
		rutile_fs_text_allocation_error(result, 0);
	}
	catch (const std::filesystem::filesystem_error& error) {
		file_read_error(result, error.code());
	}
	catch (const std::length_error&) {
		rutile_fs_text_io_error(result);
	}
}
