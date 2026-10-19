#include "platform.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <climits>
#include <fcntl.h>
#include <io.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <system_error>
#include <vector>

std::FILE* platform_standard_error() {
	return stderr;
}

std::FILE* platform_standard_input() {
	return stdin;
}

std::FILE* platform_standard_output() {
	return stdout;
}

std::FILE* platform_open_utf8_file(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

int platform_language_stdio() {
	if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
		return 0;
	}
	return _setmode(_fileno(stdout), _O_BINARY) != -1 ? 1 : 0;
}

struct PlatformLock {
	HANDLE file;
};

struct PlatformServer {
	HANDLE pipe;
	bool connected;
	bool replied;
};

std::wstring windows_wide(const char* text) {
	int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
	if (!length) {
		return {};
	}
	std::wstring wide(length, L'\0');
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide.data(), length)) {
		return {};
	}
	wide.resize(length - 1);
	return wide;
}

bool windows_utf8(const wchar_t* wide, char* text, std::size_t capacity) {
	if (capacity > INT_MAX || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, static_cast<int>(capacity), nullptr, nullptr)) {
		return false;
	}
	for (char* cursor = text; *cursor; ++cursor) {
		if (*cursor == '\\') {
			*cursor = '/';
		}
	}
	return true;
}

bool platform_current_directory(char* path, std::size_t capacity) {
	DWORD length = GetCurrentDirectoryW(0, nullptr);
	if (!length) {
		return false;
	}
	std::vector<wchar_t> wide(length);
	DWORD result = GetCurrentDirectoryW(length, wide.data());
	return result && result < length && windows_utf8(wide.data(), path, capacity);
}

bool platform_executable_path(char* path, std::size_t capacity) {
	std::vector<wchar_t> wide(32768);
	DWORD length = GetModuleFileNameW(nullptr, wide.data(), 32768);
	return length && length < 32768 && windows_utf8(wide.data(), path, capacity);
}

DWORD windows_attributes(const char* path) {
	std::wstring wide = windows_wide(path);
	return wide.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(wide.c_str());
}

bool platform_directory_exists(const char* path) {
	DWORD attributes = windows_attributes(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool platform_file_exists(const char* path) {
	DWORD attributes = windows_attributes(path);
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool platform_create_directory(const char* path) {
	std::wstring wide = windows_wide(path);
	if (wide.empty()) {
		return false;
	}
	if (CreateDirectoryW(wide.c_str(), nullptr)) {
		return true;
	}
	return GetLastError() == ERROR_ALREADY_EXISTS && platform_directory_exists(path);
}

HANDLE windows_open_file(const char* path, DWORD access, DWORD share, DWORD disposition, DWORD flags) {
	std::wstring wide = windows_wide(path);
	return wide.empty() ? INVALID_HANDLE_VALUE : CreateFileW(wide.c_str(), access, share, nullptr, disposition, flags, nullptr);
}

std::FILE* platform_open_file(const std::filesystem::path& path, const char* mode) {
	std::wstring wide_mode = windows_wide(mode);
	return _wfopen(path.c_str(), wide_mode.c_str());
}

bool platform_read_file(const char* path, char* data, std::size_t capacity) {
	if (!capacity || capacity > MAXDWORD) {
		return false;
	}
	HANDLE file = windows_open_file(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}
	LARGE_INTEGER length{};
	bool success = GetFileSizeEx(file, &length) && length.QuadPart >= 0 && static_cast<std::uint64_t>(length.QuadPart) < capacity;
	DWORD received = 0;
	if (success) {
		success = ReadFile(file, data, static_cast<DWORD>(length.QuadPart), &received, nullptr) && received == static_cast<DWORD>(length.QuadPart);
	}
	if (success) {
		data[received] = '\0';
	}
	if (!CloseHandle(file)) {
		success = false;
	}
	return success;
}

bool platform_write_file(const char* path, const char* data) {
	std::size_t length = std::strlen(data);
	std::wstring destination = windows_wide(path);
	if (length > MAXDWORD || destination.empty()) {
		return false;
	}
	std::vector<wchar_t> temporary(destination.size() + 80);
	HANDLE file = INVALID_HANDLE_VALUE;
	for (unsigned int attempt = 0; attempt < 16; ++attempt) {
		if (std::swprintf(temporary.data(), temporary.size(), L"%ls.tmp-%lu-%llu-%u", destination.c_str(), GetCurrentProcessId(), GetTickCount64(), attempt) < 0) {
			break;
		}
		file = CreateFileW(temporary.data(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) {
			break;
		}
	}
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}
	DWORD written = 0;
	bool success = WriteFile(file, data, static_cast<DWORD>(length), &written, nullptr) && written == length;
	if (!CloseHandle(file)) {
		success = false;
	}
	if (success) {
		success = false;
		for (unsigned int attempt = 0; attempt < 20; ++attempt) {
			if (MoveFileExW(temporary.data(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				success = true;
				break;
			}
			DWORD error = GetLastError();
			if (error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED) {
				break;
			}
			Sleep(10);
		}
	}
	if (!success) {
		DeleteFileW(temporary.data());
	}
	return success;
}

bool platform_endpoint(const char* root, char* endpoint, std::size_t capacity) {
	HANDLE directory = windows_open_file(root, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS);
	if (directory == INVALID_HANDLE_VALUE) {
		return false;
	}
	BY_HANDLE_FILE_INFORMATION identity{};
	bool success = GetFileInformationByHandle(directory, &identity);
	CloseHandle(directory);
	if (!success) {
		return false;
	}
	int written = std::snprintf(endpoint, capacity, "\\\\.\\pipe\\rutile-%08lx-%08lx%08lx", identity.dwVolumeSerialNumber, identity.nFileIndexHigh, identity.nFileIndexLow);
	return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

std::wstring windows_quote_argument(const std::wstring& argument) {
	std::wstring quoted = L"\"";
	std::size_t slashes = 0;
	for (wchar_t value : argument) {
		if (value == L'\\') {
			++slashes;
			continue;
		}
		quoted.append(value == L'"' ? slashes * 2 + 1 : slashes, L'\\');
		slashes = 0;
		quoted += value;
	}
	quoted.append(slashes * 2, L'\\');
	quoted += L'"';
	return quoted;
}

bool platform_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination) {
	return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool platform_spawn(const char* executable, const char* root, const char* endpoint) {
	std::wstring wide_executable = windows_wide(executable);
	std::wstring wide_root = windows_wide(root);
	std::wstring wide_endpoint = windows_wide(endpoint);
	if (wide_executable.empty() || wide_root.empty() || wide_endpoint.empty()) {
		return false;
	}
	std::wstring command = windows_quote_argument(wide_executable) + L" serve " + windows_quote_argument(wide_endpoint);
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION process{};
	if (!CreateProcessW(wide_executable.c_str(), command.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr, wide_root.c_str(), &startup, &process)) {
		return false;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

int platform_run(const std::filesystem::path& executable, const std::vector<std::string>& arguments, const std::filesystem::path& diagnostics) {
	const std::wstring& wide_executable = executable.native();
	if (wide_executable.empty() || wide_executable.find(L'\0') != std::wstring::npos) {
		std::fputs("Cannot run an empty or invalid executable path.\n", stderr);
		return -1;
	}
	std::wstring command = windows_quote_argument(wide_executable);
	for (const std::string& argument : arguments) {
		std::wstring wide_argument = windows_wide(argument.c_str());
		if (argument.find('\0') != std::string::npos || (!argument.empty() && wide_argument.empty())) {
			std::fputs("Cannot run executable: an argument is not valid UTF-8 or contains a null byte.\n", stderr);
			return -1;
		}
		command += L' ';
		command += windows_quote_argument(wide_argument);
	}
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	HANDLE diagnostic_file = INVALID_HANDLE_VALUE;
	HANDLE input_file = INVALID_HANDLE_VALUE;
	std::vector<unsigned char> attribute_storage;
	STARTUPINFOEXW extended{};
	HANDLE inherited_handles[2]{};
	DWORD creation_flags = 0;
	if (!diagnostics.empty()) {
		SECURITY_ATTRIBUTES security{ sizeof(security), nullptr, TRUE };
		diagnostic_file = CreateFileW(diagnostics.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		input_file = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		SIZE_T attribute_size = 0;
		InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
		attribute_storage.resize(attribute_size);
		extended.StartupInfo = startup;
		extended.StartupInfo.cb = sizeof(extended);
		extended.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
		if (diagnostic_file == INVALID_HANDLE_VALUE || input_file == INVALID_HANDLE_VALUE || !InitializeProcThreadAttributeList(extended.lpAttributeList, 1, 0, &attribute_size)) {
			if (diagnostic_file != INVALID_HANDLE_VALUE) {
				CloseHandle(diagnostic_file);
			}
			if (input_file != INVALID_HANDLE_VALUE) {
				CloseHandle(input_file);
			}
			return -1;
		}
		inherited_handles[0] = diagnostic_file;
		inherited_handles[1] = input_file;
		if (!UpdateProcThreadAttribute(extended.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited_handles, sizeof(inherited_handles), nullptr, nullptr)) {
			DeleteProcThreadAttributeList(extended.lpAttributeList);
			CloseHandle(diagnostic_file);
			CloseHandle(input_file);
			return -1;
		}
		extended.StartupInfo.hStdInput = input_file;
		extended.StartupInfo.hStdOutput = diagnostic_file;
		extended.StartupInfo.hStdError = diagnostic_file;
		creation_flags = CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT;
	}
	PROCESS_INFORMATION process{};
	BOOL created = CreateProcessW(wide_executable.c_str(), command.data(), nullptr, nullptr, TRUE, creation_flags, nullptr, nullptr, diagnostics.empty() ? &startup : &extended.StartupInfo, &process);
	DWORD error = GetLastError();
	if (!diagnostics.empty()) {
		DeleteProcThreadAttributeList(extended.lpAttributeList);
		CloseHandle(diagnostic_file);
		CloseHandle(input_file);
	}
	if (!created) {
		std::fprintf(stderr, "Cannot start executable: %s (Windows error %lu).\n", std::system_category().message(static_cast<int>(error)).c_str(), error);
		return -1;
	}
	CloseHandle(process.hThread);
	DWORD exit_code = 0;
	if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0 || !GetExitCodeProcess(process.hProcess, &exit_code)) {
		DWORD error = GetLastError();
		CloseHandle(process.hProcess);
		std::fprintf(stderr, "Cannot wait for executable: %s (Windows error %lu).\n", std::system_category().message(static_cast<int>(error)).c_str(), error);
		return -1;
	}
	CloseHandle(process.hProcess);
	return static_cast<int>(exit_code);
}

void platform_sleep(unsigned int milliseconds) {
	Sleep(milliseconds);
}

struct PlatformLock* platform_lock_acquire(const char* path) {
	HANDLE file = windows_open_file(path, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL);
	if (file == INVALID_HANDLE_VALUE) {
		return nullptr;
	}
	struct PlatformLock* lock = new (std::nothrow) struct PlatformLock{ file };
	if (!lock) {
		CloseHandle(file);
	}
	return lock;
}

void platform_lock_release(struct PlatformLock* lock) {
	if (lock) {
		CloseHandle(lock->file);
		delete lock;
	}
}

bool windows_pipe_transfer(HANDLE pipe, bool writing, char* data, DWORD length, DWORD* transferred, DWORD timeout) {
	OVERLAPPED operation{};
	operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!operation.hEvent) {
		return false;
	}
	BOOL success = writing ? WriteFile(pipe, data, length, transferred, &operation) : ReadFile(pipe, data, length, transferred, &operation);
	if (!success && GetLastError() == ERROR_IO_PENDING) {
		if (WaitForSingleObject(operation.hEvent, timeout) == WAIT_OBJECT_0) {
			success = GetOverlappedResult(pipe, &operation, transferred, FALSE);
		}
		else {
			CancelIoEx(pipe, &operation);
			GetOverlappedResult(pipe, &operation, transferred, TRUE);
			success = FALSE;
		}
	}
	CloseHandle(operation.hEvent);
	return success;
}

bool windows_write_line(HANDLE pipe, const char* text) {
	std::string line = std::string(text) + '\n';
	if (line.size() > MAXDWORD) {
		return false;
	}
	std::size_t offset = 0;
	while (offset < line.size()) {
		DWORD written = 0;
		if (!windows_pipe_transfer(pipe, true, line.data() + offset, static_cast<DWORD>(line.size() - offset), &written, 1000) || !written) {
			return false;
		}
		offset += written;
	}
	return true;
}

bool windows_read_line(HANDLE pipe, char* text, std::size_t capacity, unsigned int timeout_ms = 1000) {
	if (!capacity) {
		return false;
	}
	ULONGLONG deadline = GetTickCount64() + timeout_ms;
	std::size_t length = 0;
	while (true) {
		ULONGLONG now = GetTickCount64();
		char value;
		DWORD received = 0;
		if (now >= deadline || !windows_pipe_transfer(pipe, false, &value, 1, &received, static_cast<DWORD>(deadline - now)) || received != 1) {
			return false;
		}
		if (value == '\n') {
			text[length] = '\0';
			return true;
		}
		if (!value || length == capacity - 1) {
			return false;
		}
		text[length++] = value;
	}
}

bool platform_request(const char* endpoint, const char* command, char* response, std::size_t capacity, unsigned int timeout_ms) {
	std::wstring wide = windows_wide(endpoint);
	if (wide.empty()) {
		return false;
	}
	HANDLE pipe = CreateFileW(wide.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
	if (pipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY && WaitNamedPipeW(wide.c_str(), 1000)) {
		pipe = CreateFileW(wide.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
	}
	if (pipe == INVALID_HANDLE_VALUE) {
		return false;
	}
	bool success = windows_write_line(pipe, command) && windows_read_line(pipe, response, capacity, timeout_ms);
	CloseHandle(pipe);
	return success;
}

struct PlatformServer* platform_server_open(const char* endpoint) {
	std::wstring wide = windows_wide(endpoint);
	if (wide.empty()) {
		return nullptr;
	}
	HANDLE pipe = CreateNamedPipeW(wide.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 256, 256, 1000, nullptr);
	if (pipe == INVALID_HANDLE_VALUE) {
		return nullptr;
	}
	struct PlatformServer* server = new (std::nothrow) struct PlatformServer{ pipe, false, false };
	if (!server) {
		CloseHandle(pipe);
	}
	return server;
}

platform_accept platform_server_accept_for(struct PlatformServer* server, unsigned int timeout_ms) {
	OVERLAPPED operation{};
	operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!operation.hEvent) {
		return platform_accept::error;
	}
	BOOL success = ConnectNamedPipe(server->pipe, &operation);
	platform_accept result = platform_accept::error;
	if (!success) {
		DWORD error = GetLastError();
		if (error == ERROR_PIPE_CONNECTED) {
			success = TRUE;
		}
		else if (error == ERROR_IO_PENDING) {
			DWORD waited = WaitForSingleObject(operation.hEvent, timeout_ms);
			if (waited != WAIT_OBJECT_0) {
				CancelIoEx(server->pipe, &operation);
			}
			DWORD transferred = 0;
			success = GetOverlappedResult(server->pipe, &operation, &transferred, TRUE);
			if (!success && waited == WAIT_TIMEOUT && GetLastError() == ERROR_OPERATION_ABORTED) {
				result = platform_accept::timeout;
			}
		}
	}
	CloseHandle(operation.hEvent);
	server->connected = success != FALSE;
	server->replied = false;
	if (success) {
		return platform_accept::accepted;
	}
	return result;
}

bool platform_server_accept(struct PlatformServer* server) {
	return platform_server_accept_for(server, INFINITE) == platform_accept::accepted;
}

bool platform_server_read(struct PlatformServer* server, char* command, std::size_t capacity) {
	return server->connected && windows_read_line(server->pipe, command, capacity);
}

bool platform_server_write(struct PlatformServer* server, const char* response) {
	server->replied = server->connected && windows_write_line(server->pipe, response);
	return server->replied;
}

void platform_server_disconnect(struct PlatformServer* server) {
	if (server->connected) {
		if (server->replied) {
			char value;
			DWORD received = 0;
			windows_pipe_transfer(server->pipe, false, &value, 1, &received, 1000);
		}
		DisconnectNamedPipe(server->pipe);
		server->connected = false;
		server->replied = false;
	}
}

void platform_server_close(struct PlatformServer* server) {
	if (server) {
		platform_server_disconnect(server);
		CloseHandle(server->pipe);
		delete server;
	}
}

void* platform_library_open(const std::filesystem::path& path) {
	return LoadLibraryW(path.c_str());
}

void* platform_library_symbol(void* library, const char* name) {
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name));
}

void platform_library_close(void* library) {
	FreeLibrary(static_cast<HMODULE>(library));
}
