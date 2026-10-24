#pragma once

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

struct PlatformLock;
struct PlatformServer;
struct PlatformStreamServer;
struct PlatformStream;

extern "C" std::FILE* platform_standard_error();
extern "C" std::FILE* platform_standard_input();
extern "C" std::FILE* platform_standard_output();
extern "C" int platform_language_stdio();

bool platform_current_directory(char* path, std::size_t capacity);
unsigned long platform_process_id();
bool platform_executable_path(char* path, std::size_t capacity);
bool platform_directory_exists(const char* path);
bool platform_file_exists(const char* path);
bool platform_create_directory(const char* path);
std::FILE* platform_open_file(const std::filesystem::path& path, const char* mode);
extern "C" std::FILE* platform_open_utf8_file(const char* path, const char* mode);
bool platform_read_file(const char* path, char* data, std::size_t capacity);
bool platform_write_file(const char* path, const char* data);
bool platform_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination);
bool platform_endpoint(const char* root, char* endpoint, std::size_t capacity);
bool platform_spawn(const char* executable, const char* root, const char* endpoint);
int platform_run(const std::filesystem::path& executable, const std::vector<std::string>& arguments, const std::filesystem::path& diagnostics = {});
void platform_sleep(unsigned int milliseconds);

struct PlatformLock* platform_lock_acquire(const char* path);
void platform_lock_release(struct PlatformLock* lock);

bool platform_request(const char* endpoint, const char* command, char* response, std::size_t capacity, unsigned int timeout_ms = 1000);
struct PlatformServer* platform_server_open(const char* endpoint);
enum class platform_accept {
	accepted,
	timeout,
	error,
};

platform_accept platform_server_accept_for(struct PlatformServer* server, unsigned int timeout_ms);
bool platform_server_accept(struct PlatformServer* server);
bool platform_server_read(struct PlatformServer* server, char* command, std::size_t capacity);
bool platform_server_write(struct PlatformServer* server, const char* response);
void platform_server_disconnect(struct PlatformServer* server);
void platform_server_close(struct PlatformServer* server);

struct PlatformStreamServer* platform_stream_server_open(const char* endpoint);
platform_accept platform_stream_server_accept_for(struct PlatformStreamServer* server, unsigned int timeout_ms, struct PlatformStream** stream);
std::FILE* platform_stream_input(struct PlatformStream* stream);
std::FILE* platform_stream_output(struct PlatformStream* stream);
void platform_stream_close(struct PlatformStream* stream);
void platform_stream_server_interrupt(struct PlatformStreamServer* server);
void platform_stream_server_close(struct PlatformStreamServer* server);




void* platform_library_open(const std::filesystem::path& path);
void* platform_library_symbol(void* library, const char* name);
void platform_library_close(void* library);
