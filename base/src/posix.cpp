#include "platform.hpp"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <cstdint>
#include <chrono>
#include <climits>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

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
	return 1;
}

struct PlatformLock {
	int descriptor;
};

struct PlatformServer {
	int listener;
	int client;
	dev_t device;
	ino_t inode;
	char endpoint[sizeof(sockaddr_un::sun_path)];
};

bool platform_current_directory(char* path, std::size_t capacity) {
	return capacity > 0 && getcwd(path, capacity) != nullptr;
}

bool platform_executable_path(char* path, std::size_t capacity) {
#if defined(__APPLE__)
	if (capacity > UINT32_MAX) {
		return false;
	}
	std::uint32_t size = static_cast<std::uint32_t>(capacity);
	if (_NSGetExecutablePath(path, &size)) {
		return false;
	}
	char* resolved = realpath(path, nullptr);
	if (!resolved) {
		return false;
	}
	bool success = strlen(resolved) < capacity;
	if (success) {
		strcpy(path, resolved);
	}
	free(resolved);
	return success;
#elif defined(__linux__)
	if (capacity < 2) {
		return false;
	}
	ssize_t length = readlink("/proc/self/exe", path, capacity - 1);
	if (length < 0 || static_cast<std::size_t>(length) >= capacity - 1) {
		return false;
	}
	path[length] = '\0';
	return true;
#else
	static_cast<void>(path);
	static_cast<void>(capacity);
	return false;
#endif
}

bool platform_directory_exists(const char* path) {
	struct stat information;
	return !stat(path, &information) && S_ISDIR(information.st_mode);
}

bool platform_file_exists(const char* path) {
	struct stat information;
	return !stat(path, &information) && S_ISREG(information.st_mode);
}

bool platform_create_directory(const char* path) {
	return !mkdir(path, 0700) || (errno == EEXIST && platform_directory_exists(path));
}

std::FILE* platform_open_file(const std::filesystem::path& path, const char* mode) {
	return fopen(path.c_str(), mode);
}

bool platform_read_file(const char* path, char* data, std::size_t capacity) {
	if (!capacity) {
		return false;
	}
	FILE* file = fopen(path, "rb");
	if (!file) {
		return false;
	}
	std::size_t length = fread(data, 1, capacity - 1, file);
	bool success = !ferror(file) && fgetc(file) == EOF && !ferror(file);
	data[length] = '\0';
	return !fclose(file) && success;
}

bool platform_write_file(const char* path, const char* data) {
	std::size_t path_length = strlen(path);
	if (path_length > SIZE_MAX - sizeof(".XXXXXX")) {
		return false;
	}
	char* temporary = new (std::nothrow) char[path_length + sizeof(".XXXXXX")];
	if (!temporary) {
		return false;
	}
	memcpy(temporary, path, path_length);
	memcpy(temporary + path_length, ".XXXXXX", sizeof(".XXXXXX"));
	int descriptor = mkstemp(temporary);
	if (descriptor < 0) {
		delete[] temporary;
		return false;
	}
	std::size_t length = strlen(data);
	std::size_t offset = 0;
	bool success = true;
	while (offset < length) {
		ssize_t written = write(descriptor, data + offset, length - offset);
		if (written < 0 && errno == EINTR) {
			continue;
		}
		if (written <= 0) {
			success = false;
			break;
		}
		offset += static_cast<std::size_t>(written);
	}
	if (close(descriptor)) {
		success = false;
	}
	if (success) {
		success = !rename(temporary, path);
	}
	if (!success) {
		unlink(temporary);
	}
	delete[] temporary;
	return success;
}

bool platform_endpoint(const char* root, char* endpoint, std::size_t capacity) {
	std::uint64_t hash = UINT64_C(14695981039346656037);
	for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(root); *cursor; ++cursor) {
		hash = (hash ^ *cursor) * UINT64_C(1099511628211);
	}
	int length = snprintf(endpoint, capacity, "/tmp/rutile-%lu-%016llx.sock", static_cast<unsigned long>(getuid()), static_cast<unsigned long long>(hash));
	return length > 0 && static_cast<std::size_t>(length) < capacity && static_cast<std::size_t>(length) < sizeof(sockaddr_un::sun_path);
}

bool platform_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination) {
	return !rename(source.c_str(), destination.c_str());
}

bool platform_spawn(const char* executable, const char* root, const char* endpoint) {
	int descriptors[2];
	if (pipe(descriptors)) {
		return false;
	}
	if (fcntl(descriptors[1], F_SETFD, FD_CLOEXEC) < 0) {
		close(descriptors[0]);
		close(descriptors[1]);
		return false;
	}
	pid_t child = fork();
	if (!child) {
		close(descriptors[0]);
		if (setsid() >= 0) {
			pid_t daemon = fork();
			if (daemon > 0) {
				_exit(0);
			}
			if (!daemon && !chdir(root)) {
				int null_file = open("/dev/null", O_RDWR);
				if (null_file >= 0 && dup2(null_file, STDIN_FILENO) >= 0 && dup2(null_file, STDOUT_FILENO) >= 0 && dup2(null_file, STDERR_FILENO) >= 0) {
					if (null_file > STDERR_FILENO) {
						close(null_file);
					}
					execl(executable, executable, "serve", endpoint, static_cast<char*>(nullptr));
				}
			}
		}
		int error = errno;
		while (write(descriptors[1], &error, sizeof(error)) < 0 && errno == EINTR) {
		}
		_exit(1);
	}
	close(descriptors[1]);
	if (child < 0) {
		close(descriptors[0]);
		return false;
	}
	int status = 0;
	pid_t waited;
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	int error;
	ssize_t length;
	do {
		length = read(descriptors[0], &error, sizeof(error));
	} while (length < 0 && errno == EINTR);
	close(descriptors[0]);
	return waited == child && WIFEXITED(status) && !WEXITSTATUS(status) && !length;
}

int platform_run(const std::filesystem::path& executable, const std::vector<std::string>& arguments, const std::filesystem::path& diagnostics) {
	std::string executable_name = executable.native();
	if (executable_name.empty() || executable_name.find('\0') != std::string::npos) {
		fprintf(stderr, "Cannot run an empty or invalid executable path.\n");
		return -1;
	}
	std::vector<char*> values{ executable_name.data() };
	for (const std::string& argument : arguments) {
		if (argument.find('\0') != std::string::npos) {
			fprintf(stderr, "Cannot run executable: an argument contains a null byte.\n");
			return -1;
		}
		values.push_back(const_cast<char*>(argument.c_str()));
	}
	values.push_back(nullptr);
	int diagnostic_file = -1;
	int input_file = -1;
	if (!diagnostics.empty()) {
		diagnostic_file = open(diagnostics.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		input_file = open("/dev/null", O_RDONLY | O_CLOEXEC);
		if (diagnostic_file < 0 || input_file < 0) {
			if (diagnostic_file >= 0) {
				close(diagnostic_file);
			}
			if (input_file >= 0) {
				close(input_file);
			}
			return -1;
		}
		int diagnostic_copy = fcntl(diagnostic_file, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
		int input_copy = fcntl(input_file, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
		close(diagnostic_file);
		close(input_file);
		if (diagnostic_copy < 0 || input_copy < 0) {
			if (diagnostic_copy >= 0) {
				close(diagnostic_copy);
			}
			if (input_copy >= 0) {
				close(input_copy);
			}
			return -1;
		}
		diagnostic_file = diagnostic_copy;
		input_file = input_copy;
	}
	pid_t child = fork();
	if (child < 0) {
		if (diagnostic_file >= 0) {
			close(diagnostic_file);
			close(input_file);
		}
		fprintf(stderr, "Cannot start executable: %s.\n", strerror(errno));
		return -1;
	}
	if (!child) {
		if (diagnostic_file >= 0) {
			if (dup2(input_file, STDIN_FILENO) < 0 || dup2(diagnostic_file, STDOUT_FILENO) < 0 || dup2(diagnostic_file, STDERR_FILENO) < 0) {
				_exit(127);
			}
			if (diagnostic_file > STDERR_FILENO) {
				close(diagnostic_file);
			}
			if (input_file > STDERR_FILENO) {
				close(input_file);
			}
		}
		execv(executable_name.c_str(), values.data());
		fprintf(stderr, "Cannot execute '%s': %s.\n", executable_name.c_str(), strerror(errno));
		_exit(127);
	}
	if (diagnostic_file >= 0) {
		close(diagnostic_file);
		close(input_file);
	}
	int status = 0;
	pid_t waited;
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	if (waited < 0) {
		fprintf(stderr, "Cannot wait for executable: %s.\n", strerror(errno));
		return -1;
	}
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		return 128 + WTERMSIG(status);
	}
	fprintf(stderr, "Executable ended without an exit status.\n");
	return -1;
}

void platform_sleep(unsigned int milliseconds) {
	struct timespec duration = { static_cast<time_t>(milliseconds / 1000), static_cast<long>(milliseconds % 1000) * 1000000L };
	while (nanosleep(&duration, &duration) < 0 && errno == EINTR) {
	}
}

struct PlatformLock* platform_lock_acquire(const char* path) {
	int descriptor = open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
	if (descriptor < 0) {
		return nullptr;
	}
	if (flock(descriptor, LOCK_EX | LOCK_NB)) {
		close(descriptor);
		return nullptr;
	}
	struct PlatformLock* lock = new (std::nothrow) struct PlatformLock{};
	if (!lock) {
		close(descriptor);
		return nullptr;
	}
	lock->descriptor = descriptor;
	return lock;
}

void platform_lock_release(struct PlatformLock* lock) {
	if (lock) {
		close(lock->descriptor);
		delete lock;
	}
}

std::int64_t posix_milliseconds() {
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now)) {
		return -1;
	}
	return static_cast<std::int64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

bool posix_wait(int descriptor, short events, std::int64_t deadline) {
	while (true) {
		std::int64_t remaining = deadline - posix_milliseconds();
		if (remaining <= 0) {
			return false;
		}
		struct pollfd state = { descriptor, events, 0 };
		int result = poll(&state, 1, static_cast<int>(remaining));
		if (result < 0 && errno == EINTR) {
			continue;
		}
		return result > 0 && (state.revents & events);
	}
}

int posix_socket() {
	int descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
	if (descriptor < 0) {
		return -1;
	}
	if (fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0 || fcntl(descriptor, F_SETFL, O_NONBLOCK) < 0) {
		close(descriptor);
		return -1;
	}
#if defined(SO_NOSIGPIPE)
	int enabled = 1;
	if (setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled))) {
		close(descriptor);
		return -1;
	}
#endif
	return descriptor;
}

bool posix_address(const char* endpoint, struct sockaddr_un* address) {
	if (strlen(endpoint) >= sizeof(address->sun_path)) {
		return false;
	}
	memset(address, 0, sizeof(*address));
	address->sun_family = AF_UNIX;
	strcpy(address->sun_path, endpoint);
	return true;
}

bool posix_write_line(int descriptor, const char* text) {
	std::size_t length = strlen(text);
	if (strchr(text, '\n') || strchr(text, '\r')) {
		return false;
	}
	std::int64_t deadline = posix_milliseconds() + 1000;
	std::size_t offset = 0;
	while (offset <= length) {
		if (!posix_wait(descriptor, POLLOUT, deadline)) {
			return false;
		}
		const char* data = offset == length ? "\n" : text + offset;
		std::size_t size = offset == length ? 1 : length - offset;
		int flags = 0;
#if defined(MSG_NOSIGNAL)
		flags = MSG_NOSIGNAL;
#endif
		ssize_t written = send(descriptor, data, size, flags);
		if (written < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
			continue;
		}
		if (written <= 0) {
			return false;
		}
		offset += static_cast<std::size_t>(written);
	}
	return true;
}

bool posix_read_line(int descriptor, char* text, std::size_t capacity, unsigned int timeout_ms = 1000) {
	if (!capacity) {
		return false;
	}
	std::int64_t deadline = posix_milliseconds() + timeout_ms;
	std::size_t length = 0;
	while (true) {
		if (!posix_wait(descriptor, POLLIN, deadline)) {
			return false;
		}
		char character;
		ssize_t received = recv(descriptor, &character, 1, 0);
		if (received < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
			continue;
		}
		if (received <= 0) {
			return false;
		}
		if (character == '\n') {
			text[length] = '\0';
			return true;
		}
		if (!character || character == '\r' || length + 1 >= capacity) {
			return false;
		}
		text[length++] = character;
	}
}

bool platform_request(const char* endpoint, const char* command, char* response, std::size_t capacity, unsigned int timeout_ms) {
	struct sockaddr_un address;
	if (!posix_address(endpoint, &address)) {
		return false;
	}
	int descriptor = posix_socket();
	if (descriptor < 0) {
		return false;
	}
	bool connected = !connect(descriptor, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
	if (!connected && errno == EINPROGRESS && posix_wait(descriptor, POLLOUT, posix_milliseconds() + 1000)) {
		int error = 0;
		socklen_t size = sizeof(error);
		connected = !getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &error, &size) && !error;
	}
	bool success = connected && posix_write_line(descriptor, command) && posix_read_line(descriptor, response, capacity, timeout_ms);
	close(descriptor);
	return success;
}

struct PlatformServer* platform_server_open(const char* endpoint) {
	struct sockaddr_un address;
	if (!posix_address(endpoint, &address)) {
		return nullptr;
	}
	int listener = posix_socket();
	if (listener < 0) {
		return nullptr;
	}
	struct PlatformServer* server = new (std::nothrow) struct PlatformServer{};
	if (!server) {
		close(listener);
		return nullptr;
	}
	struct stat existing;
	bool exists = !lstat(endpoint, &existing);
	if ((exists && (!S_ISSOCK(existing.st_mode) || existing.st_uid != getuid())) || (!exists && errno != ENOENT)) {
		close(listener);
		delete server;
		return nullptr;
	}
	if (exists && unlink(endpoint)) {
		close(listener);
		delete server;
		return nullptr;
	}
	if (bind(listener, reinterpret_cast<struct sockaddr*>(&address), sizeof(address))) {
		close(listener);
		delete server;
		return nullptr;
	}
	struct stat bound;
	if (lstat(endpoint, &bound) || chmod(endpoint, 0600) || listen(listener, 8)) {
		close(listener);
		unlink(endpoint);
		delete server;
		return nullptr;
	}
	server->listener = listener;
	server->client = -1;
	server->device = bound.st_dev;
	server->inode = bound.st_ino;
	strcpy(server->endpoint, endpoint);
	return server;
}

platform_accept platform_server_accept_for(struct PlatformServer* server, unsigned int timeout_ms) {
	platform_server_disconnect(server);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
	while (true) {
		struct pollfd state = { server->listener, POLLIN, 0 };
		int wait = -1;
		if (timeout_ms != UINT_MAX) {
			const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
			wait = remaining <= 0 ? 0 : remaining > INT_MAX ? INT_MAX : static_cast<int>(remaining);
		}
		int result = poll(&state, 1, wait);
		if (!result) {
			return platform_accept::timeout;
		}
		if (result < 0 && errno == EINTR) {
			continue;
		}
		if (result <= 0 || !(state.revents & POLLIN)) {
			return platform_accept::error;
		}
		int client = accept(server->listener, nullptr, nullptr);
		if (client < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
			continue;
		}
		if (client < 0) {
			return platform_accept::error;
		}
		if (fcntl(client, F_SETFD, FD_CLOEXEC) < 0 || fcntl(client, F_SETFL, O_NONBLOCK) < 0) {
			close(client);
			return platform_accept::error;
		}
#if defined(SO_NOSIGPIPE)
		int enabled = 1;
		if (setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled))) {
			close(client);
			return platform_accept::error;
		}
#endif
		server->client = client;
		return platform_accept::accepted;
	}
}

bool platform_server_accept(struct PlatformServer* server) {
	return platform_server_accept_for(server, UINT_MAX) == platform_accept::accepted;
}

bool platform_server_read(struct PlatformServer* server, char* command, std::size_t capacity) {
	return server->client >= 0 && posix_read_line(server->client, command, capacity);
}

bool platform_server_write(struct PlatformServer* server, const char* response) {
	return server->client >= 0 && posix_write_line(server->client, response);
}

void platform_server_disconnect(struct PlatformServer* server) {
	if (server->client >= 0) {
		close(server->client);
		server->client = -1;
	}
}

void platform_server_close(struct PlatformServer* server) {
	if (server) {
		platform_server_disconnect(server);
		close(server->listener);
		struct stat current;
		if (!lstat(server->endpoint, &current) && current.st_dev == server->device && current.st_ino == server->inode) {
			unlink(server->endpoint);
		}
		delete server;
	}
}
