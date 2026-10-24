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
#include <new>
#include <string>
#include <thread>
#include <vector>

struct ProcessBuffer {
	const char* data;
	std::size_t size;
};

struct ProcessOutput {
	int state;
	int exit_code;
	ProcessBuffer output;
	ProcessBuffer error;
};

struct NativeProcess {
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
	std::string read_bytes;
	ProcessBuffer read_view {};
	ProcessOutput result {};
	std::size_t read_cursor = 0;
	int state = 1;
	int exit_code = -1;
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

void process_read_pipe(NativeProcess* process, HANDLE pipe, bool standard_output) {
	char bytes[4096];
	DWORD count = 0;
	while (ReadFile(pipe, bytes, sizeof(bytes), &count, nullptr) && count) {
		{
			std::lock_guard lock(process->mutex);
			auto& destination = standard_output ? process->output_bytes : process->error_bytes;
			destination.append(bytes, count);
		}
		process->changed.notify_all();
	}
	if (standard_output) {
		{
			std::lock_guard lock(process->mutex);
			process->output_closed = true;
		}
		process->changed.notify_all();
	}
}

void process_write_pipe(NativeProcess* process) {
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

void process_join(NativeProcess* process) {
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

extern "C" void rutile_process_dispose(NativeProcess* process) {
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

extern "C" NativeProcess* rutile_process_start(const char* const* arguments, std::size_t count, const char* directory, const char* const* environment, std::size_t environment_count) {
	if (!count || !arguments || !arguments[0] || !*arguments[0]) {
		return nullptr;
	}
	auto* process = new (std::nothrow) NativeProcess;
	if (!process) {
		return nullptr;
	}
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
		wchar_t* inherited = GetEnvironmentStringsW();
		if (!inherited) {
			delete process;
			return nullptr;
		}
		for (const wchar_t* cursor = inherited; *cursor;) {
			std::wstring entry(cursor);
			cursor += entry.size() + 1;
			auto separator = entry.find(L'=', 1);
			if (separator != std::wstring::npos) {
				variables[entry.substr(0, separator)] = entry.substr(separator + 1);
			}
		}
		FreeEnvironmentStringsW(inherited);
		for (std::size_t index = 0; index < environment_count; ++index) {
			auto entry = process_wide(environment[index]);
			auto separator = entry.find(L'=');
			if (separator == std::wstring::npos || !separator) {
				delete process;
				return nullptr;
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
	HANDLE child_input = nullptr;
	HANDLE child_output = nullptr;
	HANDLE child_error = nullptr;
	if (!CreatePipe(&child_input, &process->input, &security, 0)
		|| !CreatePipe(&process->output, &child_output, &security, 0)
		|| !CreatePipe(&process->error, &child_error, &security, 0)) {
		for (HANDLE handle : { child_input, child_output, child_error }) {
			if (handle) {
				CloseHandle(handle);
			}
		}
		rutile_process_dispose(process);
		return nullptr;
	}
	SetHandleInformation(process->input, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(process->output, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(process->error, HANDLE_FLAG_INHERIT, 0);
	STARTUPINFOEXW startup {};
	startup.StartupInfo.cb = sizeof(startup);
	startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	startup.StartupInfo.hStdInput = child_input;
	startup.StartupInfo.hStdOutput = child_output;
	startup.StartupInfo.hStdError = child_error;
	SIZE_T attribute_size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
	std::vector<std::byte> attribute_storage(attribute_size);
	startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
	bool initialized = InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attribute_size) != FALSE;
	HANDLE inherited[] { child_input, child_output, child_error };
	bool configured = initialized && UpdateProcThreadAttribute(startup.lpAttributeList, 0,
		PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr) != FALSE;
	PROCESS_INFORMATION information {};
	bool started = configured && CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
		EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, environment_count ? environment_block.data() : nullptr,
		working_directory.empty() ? nullptr : working_directory.c_str(), &startup.StartupInfo, &information) != FALSE;
	if (initialized) {
		DeleteProcThreadAttributeList(startup.lpAttributeList);
	}
	CloseHandle(child_input);
	CloseHandle(child_output);
	CloseHandle(child_error);
	if (!started) {
		rutile_process_dispose(process);
		return nullptr;
	}
	process->process = information.hProcess;
	process->job = CreateJobObjectW(nullptr, nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!process->job || !SetInformationJobObject(process->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))
		|| !AssignProcessToJobObject(process->job, process->process)) {
		TerminateProcess(process->process, 1);
		CloseHandle(information.hThread);
		rutile_process_dispose(process);
		return nullptr;
	}
	process->state = 0;
	process->output_thread = std::thread(process_read_pipe, process, process->output, true);
	process->error_thread = std::thread(process_read_pipe, process, process->error, false);
	process->input_thread = std::thread(process_write_pipe, process);
	ResumeThread(information.hThread);
	CloseHandle(information.hThread);
	return process;
}

extern "C" bool rutile_process_write(NativeProcess* process, const char* bytes, std::size_t size) {
	std::lock_guard lock(process->mutex);
	if (process->input_closed || process->finished) {
		return false;
	}
	process->pending_input.emplace_back(bytes, size);
	process->changed.notify_all();
	return true;
}

extern "C" void rutile_process_close_input(NativeProcess* process) {
	{
		std::lock_guard lock(process->mutex);
		process->input_closed = true;
	}
	process->changed.notify_all();
}

extern "C" const ProcessBuffer* rutile_process_read(NativeProcess* process, std::uint32_t timeout_ms) {
	std::unique_lock lock(process->mutex);
	process->changed.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
		return process->output_closed || process->read_cursor < process->output_bytes.size();
	});
	process->read_bytes = process->output_bytes.substr(process->read_cursor);
	process->read_cursor = process->output_bytes.size();
	process->read_view = { process->read_bytes.data(), process->read_bytes.size() };
	return &process->read_view;
}

void process_wait(NativeProcess* process, std::uint32_t timeout_ms) {
	if (process->finished) {
		return;
	}
	DWORD waited = WaitForSingleObject(process->process, timeout_ms);
	if (waited == WAIT_TIMEOUT) {
		process->state = 2;
		TerminateJobObject(process->job, 1);
		WaitForSingleObject(process->process, INFINITE);
	}
	else if (waited != WAIT_OBJECT_0) {
		process->state = 1;
		TerminateJobObject(process->job, 1);
		WaitForSingleObject(process->process, INFINITE);
	}
	DWORD exit_code = 0;
	GetExitCodeProcess(process->process, &exit_code);
	process->exit_code = static_cast<int>(exit_code);
	TerminateJobObject(process->job, 1);
	process_join(process);
	process->finished = true;
	return;
}

extern "C" const ProcessOutput* rutile_process_wait(NativeProcess* process, std::uint32_t timeout_ms) {
	process_wait(process, timeout_ms);
	process->result = {
		process->state,
		process->exit_code,
		{ process->output_bytes.data(), process->output_bytes.size() },
		{ process->error_bytes.data(), process->error_bytes.size() }
	};
	return &process->result;
}
