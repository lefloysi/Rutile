#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <system_error>
#include <vector>
#include <variant>
#include <stdexcept>

struct Process;
extern "C" {
void rutile_process_start_complete(void* result, Process* process);
void rutile_process_start_launch_error(void* result);
void rutile_process_start_allocation_error(void* result, std::size_t bytes);
void rutile_process_write_complete(void* result);
void rutile_process_write_io_error(void* result);
void rutile_process_write_allocation_error(void* result, std::size_t bytes);
std::size_t rutile_process_read_complete(void* result, const char* bytes, std::size_t size);
void rutile_process_read_io_error(void* result);
void rutile_process_read_allocation_error(void* result, std::size_t bytes);
void rutile_process_wait_complete(void* result, int exit_code, const char* output, std::size_t output_size, const char* error, std::size_t error_size);
void rutile_process_wait_timeout_error(void* result);
void rutile_process_wait_io_error(void* result);
void rutile_process_wait_allocation_error(void* result, std::size_t bytes);
}

struct AllocationFailure {
	std::size_t requested_bytes;
};
struct IoFailure {
};
struct TimeoutFailure {
};
struct LaunchFailure {
};
using AsyncFailure = std::variant<std::monostate, AllocationFailure, IoFailure>;
using Completion = std::variant<std::monostate, TimeoutFailure, IoFailure>;

struct Process {
	HANDLE process = nullptr;
	HANDLE job = nullptr;
	HANDLE input = nullptr;
	HANDLE output = nullptr;
	HANDLE error = nullptr;
	std::thread input_thread;
	std::thread output_thread;
	std::thread error_thread;
	std::mutex mutex;
	std::condition_variable changed;
	std::deque<std::string> pending_input;
	bool input_closed = false;
	bool output_closed = false;
	bool finished = false;
	std::string output_bytes;
	std::string error_bytes;
	std::size_t read_cursor = 0;
	int exit_code = 0;
	AsyncFailure failure;
	Completion completion;
};

struct ProcessHandle {
	HANDLE value = nullptr;
	~ProcessHandle() {
		if (value) {
			CloseHandle(value);
		}
	}
};

std::wstring process_wide(const char* text) {
	int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
	if (!count) {
		return {};
	}
	std::wstring result(static_cast<std::size_t>(count), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), count);
	result.pop_back();
	return result;
}

std::wstring process_quote(const std::wstring& argument) {
	if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
		return argument;
	}
	std::wstring result = L"\"";
	std::size_t slashes = 0;
	for (wchar_t character : argument) {
		if (character == L'\\') {
			++slashes;
			continue;
		}
		result.append(slashes * (character == L'"' ? 2 : 1), L'\\');
		slashes = 0;
		if (character == L'"') {
			result += L'\\';
		}
		result += character;
	}
	result.append(slashes * 2, L'\\');
	result += L'"';
	return result;
}

void process_read_pipe(Process* process, HANDLE pipe, bool standard_output) {
	char bytes[4096];
	DWORD count = 0;
	BOOL read = TRUE;
	while ((read = ReadFile(pipe, bytes, sizeof(bytes), &count, nullptr)) && count) {
		{
			std::lock_guard lock(process->mutex);
			auto& destination = standard_output ? process->output_bytes : process->error_bytes;
			if (std::holds_alternative<std::monostate>(process->failure)) {
				try {
					destination.append(bytes, count);
				}
				catch (const std::bad_alloc&) {
					process->failure = AllocationFailure { destination.size() + count };
				}
				catch (const std::length_error&) {
					process->failure = IoFailure {};
				}
			}
		}
		process->changed.notify_all();
	}
	const auto error = GetLastError();
	if (!read && error != ERROR_BROKEN_PIPE) {
		std::lock_guard lock(process->mutex);
		if (std::holds_alternative<std::monostate>(process->failure)) {
			process->failure = IoFailure {};
		}
	}
	if (standard_output) {
		{
			std::lock_guard lock(process->mutex);
			process->output_closed = true;
		}
		process->changed.notify_all();
	}
}

void process_write_pipe(Process* process) {
	while (true) {
		std::string bytes;
		{
			std::unique_lock lock(process->mutex);
			process->changed.wait(lock, [&] { return process->input_closed || !process->pending_input.empty(); });
			if (process->pending_input.empty()) {
				break;
			}
			bytes = std::move(process->pending_input.front());
			process->pending_input.pop_front();
		}
		std::size_t offset = 0;
		while (offset < bytes.size()) {
			DWORD count = 0;
			DWORD requested = static_cast<DWORD>((std::min)(bytes.size() - offset, std::size_t(65536)));
			if (!WriteFile(process->input, bytes.data() + offset, requested, &count, nullptr) || !count) {
				{
					std::lock_guard lock(process->mutex);
					if (std::holds_alternative<std::monostate>(process->failure)) {
						process->failure = IoFailure {};
					}
				}
				CloseHandle(process->input);
				process->input = nullptr;
				return;
			}
			offset += count;
		}
	}
	CloseHandle(process->input);
	process->input = nullptr;
}

void process_join(Process* process) {
	{
		std::lock_guard lock(process->mutex);
		process->input_closed = true;
	}
	process->changed.notify_all();
	if (process->input_thread.joinable()) {
		process->input_thread.join();
	}
	if (process->output_thread.joinable()) {
		process->output_thread.join();
	}
	if (process->error_thread.joinable()) {
		process->error_thread.join();
	}
}

extern "C" void rutile_process_dispose(Process* process) {
	if (!process) {
		return;
	}
	if (process->job) {
		TerminateJobObject(process->job, 1);
	}
	if (process->process) {
		WaitForSingleObject(process->process, INFINITE);
	}
	process_join(process);
	for (HANDLE handle : { process->input, process->output, process->error, process->process, process->job }) {
		if (handle) {
			CloseHandle(handle);
		}
	}
	delete process;
}

using ProcessOwner = std::unique_ptr<Process, decltype(&rutile_process_dispose)>;
using StartOutcome = std::variant<ProcessOwner, LaunchFailure>;

StartOutcome process_start(const char* const* arguments, std::size_t count, const char* directory, const char* const* environment, std::size_t environment_count) {
	if (!count || !arguments || !arguments[0] || !*arguments[0]) {
		return LaunchFailure {};
	}
	ProcessOwner owner(new Process, rutile_process_dispose);
	auto* process = owner.get();
	std::wstring command;
	for (std::size_t index = 0; index < count; ++index) {
		if (index) {
			command += L' ';
		}
		command += process_quote(process_wide(arguments[index]));
	}
	std::wstring working_directory = process_wide(directory);
	std::vector<wchar_t> environment_block;
	if (environment_count) {
		struct EnvironmentOrder {
			bool operator()(const std::wstring& first, const std::wstring& second) const {
				return CompareStringOrdinal(first.data(), static_cast<int>(first.size()), second.data(), static_cast<int>(second.size()), TRUE) == CSTR_LESS_THAN;
			}
		};
		std::map<std::wstring, std::wstring, EnvironmentOrder> variables;
		std::unique_ptr<wchar_t, decltype(&FreeEnvironmentStringsW)> inherited(GetEnvironmentStringsW(), FreeEnvironmentStringsW);
		if (!inherited) {
			return LaunchFailure {};
		}
		for (const wchar_t* cursor = inherited.get(); *cursor;) {
			std::wstring entry(cursor);
			cursor += entry.size() + 1;
			auto separator = entry.find(L'=', 1);
			if (separator != std::wstring::npos) {
				variables[entry.substr(0, separator)] = entry.substr(separator + 1);
			}
		}
		for (std::size_t index = 0; index < environment_count; ++index) {
			auto entry = process_wide(environment[index]);
			auto separator = entry.find(L'=');
			if (separator == std::wstring::npos || !separator) {
				return LaunchFailure {};
			}
			variables[entry.substr(0, separator)] = entry.substr(separator + 1);
		}
		for (const auto& [name, value] : variables) {
			environment_block.insert(environment_block.end(), name.begin(), name.end());
			environment_block.push_back(L'=');
			environment_block.insert(environment_block.end(), value.begin(), value.end());
			environment_block.push_back(L'\0');
		}
		environment_block.push_back(L'\0');
	}
	SECURITY_ATTRIBUTES security { sizeof(security), nullptr, TRUE };
	ProcessHandle child_input;
	ProcessHandle child_output;
	ProcessHandle child_error;
	if (!CreatePipe(&child_input.value, &process->input, &security, 0)
		|| !CreatePipe(&process->output, &child_output.value, &security, 0)
		|| !CreatePipe(&process->error, &child_error.value, &security, 0)) {
		return LaunchFailure {};
	}
	if (!SetHandleInformation(process->input, HANDLE_FLAG_INHERIT, 0)
		|| !SetHandleInformation(process->output, HANDLE_FLAG_INHERIT, 0)
		|| !SetHandleInformation(process->error, HANDLE_FLAG_INHERIT, 0)) {
		return LaunchFailure {};
	}
	STARTUPINFOEXW startup {};
	startup.StartupInfo.cb = sizeof(startup);
	startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	startup.StartupInfo.hStdInput = child_input.value;
	startup.StartupInfo.hStdOutput = child_output.value;
	startup.StartupInfo.hStdError = child_error.value;
	SIZE_T attribute_size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
	std::vector<std::byte> attribute_storage(attribute_size);
	startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
	bool initialized = InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attribute_size) != FALSE;
	HANDLE inherited[] { child_input.value, child_output.value, child_error.value };
	bool configured = initialized && UpdateProcThreadAttribute(startup.lpAttributeList, 0,
		PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr) != FALSE;
	PROCESS_INFORMATION information {};
	bool started = configured && CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
		EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, environment_count ? environment_block.data() : nullptr,
		working_directory.empty() ? nullptr : working_directory.c_str(), &startup.StartupInfo, &information) != FALSE;
	if (initialized) {
		DeleteProcThreadAttributeList(startup.lpAttributeList);
	}
	CloseHandle(child_input.value);
	child_input.value = nullptr;
	CloseHandle(child_output.value);
	child_output.value = nullptr;
	CloseHandle(child_error.value);
	child_error.value = nullptr;
	if (!started) {
		return LaunchFailure {};
	}
	ProcessHandle primary_thread;
	primary_thread.value = information.hThread;
	process->process = information.hProcess;
	process->job = CreateJobObjectW(nullptr, nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!process->job || !SetInformationJobObject(process->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))
		|| !AssignProcessToJobObject(process->job, process->process)) {
		TerminateProcess(process->process, 1);
		return LaunchFailure {};
	}
	process->output_thread = std::thread(process_read_pipe, process, process->output, true);
	process->error_thread = std::thread(process_read_pipe, process, process->error, false);
	process->input_thread = std::thread(process_write_pipe, process);
	if (ResumeThread(information.hThread) == static_cast<DWORD>(-1)) {
		return LaunchFailure {};
	}
	return StartOutcome(std::move(owner));
}

extern "C" void rutile_process_start(const char* const* arguments, std::size_t count, const char* directory, const char* const* environment, std::size_t environment_count, void* result) {
	try {
		auto started = process_start(arguments, count, directory, environment, environment_count);
		if (auto* owner = std::get_if<ProcessOwner>(&started)) {
			rutile_process_start_complete(result, owner->get());
			owner->release();
		}
		else {
			rutile_process_start_launch_error(result);
		}
	}
	catch (const std::bad_alloc&) {
		rutile_process_start_allocation_error(result, 0);
	}
	catch (const std::system_error&) {
		rutile_process_start_launch_error(result);
	}
	catch (const std::length_error&) {
		rutile_process_start_launch_error(result);
	}
}

extern "C" void rutile_process_write(Process* process, const char* bytes, std::size_t size, void* result) {
	std::lock_guard lock(process->mutex);
	if (auto* failure = std::get_if<AllocationFailure>(&process->failure)) {
		rutile_process_write_allocation_error(result, failure->requested_bytes);
		return;
	}
	if (std::holds_alternative<IoFailure>(process->failure) || process->input_closed || process->finished) {
		rutile_process_write_io_error(result);
		return;
	}
	try {
		process->pending_input.emplace_back(size ? bytes : "", size);
	}
	catch (const std::bad_alloc&) {
		rutile_process_write_allocation_error(result, size);
		return;
	}
	catch (const std::length_error&) {
		rutile_process_write_io_error(result);
		return;
	}
	process->changed.notify_all();
	rutile_process_write_complete(result);
}

extern "C" void rutile_process_close_input(Process* process) {
	{
		std::lock_guard lock(process->mutex);
		process->input_closed = true;
	}
	process->changed.notify_all();
}

extern "C" void rutile_process_read(Process* process, std::uint32_t timeout_ms, void* result) {
	std::unique_lock lock(process->mutex);
	process->changed.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
		return !std::holds_alternative<std::monostate>(process->failure) || process->output_closed || process->read_cursor < process->output_bytes.size();
	});
	if (auto* failure = std::get_if<AllocationFailure>(&process->failure)) {
		rutile_process_read_allocation_error(result, failure->requested_bytes);
		return;
	}
	if (std::holds_alternative<IoFailure>(process->failure)) {
		rutile_process_read_io_error(result);
		return;
	}
	const auto begin = process->read_cursor;
	process->read_cursor += rutile_process_read_complete(result, process->output_bytes.data() + begin, process->output_bytes.size() - begin);
}

void process_wait(Process* process, std::uint32_t timeout_ms) {
	if (process->finished) {
		return;
	}
	DWORD waited = WaitForSingleObject(process->process, timeout_ms);
	if (waited == WAIT_TIMEOUT) {
		process->completion = TimeoutFailure {};
		TerminateJobObject(process->job, 1);
		WaitForSingleObject(process->process, INFINITE);
	}
	else if (waited != WAIT_OBJECT_0) {
		process->completion = IoFailure {};
		TerminateJobObject(process->job, 1);
		WaitForSingleObject(process->process, INFINITE);
	}
	DWORD exit_code = 0;
	if (GetExitCodeProcess(process->process, &exit_code)) {
		process->exit_code = static_cast<int>(exit_code);
	}
	else {
		process->completion = IoFailure {};
	}
	TerminateJobObject(process->job, 1);
	process_join(process);
	process->finished = true;
}

extern "C" void rutile_process_wait(Process* process, std::uint32_t timeout_ms, void* result) {
	process_wait(process, timeout_ms);
	if (auto* failure = std::get_if<AllocationFailure>(&process->failure)) {
		rutile_process_wait_allocation_error(result, failure->requested_bytes);
	}
	else if (std::holds_alternative<IoFailure>(process->failure) || std::holds_alternative<IoFailure>(process->completion)) {
		rutile_process_wait_io_error(result);
	}
	else if (std::holds_alternative<TimeoutFailure>(process->completion)) {
		rutile_process_wait_timeout_error(result);
	}
	else {
		rutile_process_wait_complete(result, process->exit_code, process->output_bytes.data(), process->output_bytes.size(), process->error_bytes.data(), process->error_bytes.size());
	}
}
