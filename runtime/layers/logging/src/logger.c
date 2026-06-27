#include "procs.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#if defined(_WIN32)
#define RTLOG_THREAD_LOCAL __declspec(thread)
#else
#define RTLOG_THREAD_LOCAL _Thread_local
#endif

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static PFN_rtOutput rtlog_output = NULL;
static void *rtlog_output_user_data = NULL;

static void rtlog_default_output(const char *message, void *user_data) {
	fputs(message, stdout);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtlog_set_output(PFN_rtOutput output, void *user_data) {
	rtlog_output = output;
	rtlog_output_user_data = user_data;
}

void rtlog_printf(const char *format, ...) {
	char message[1024];
	va_list args;
	PFN_rtOutput output = rtlog_output ? rtlog_output : rtlog_default_output;

	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	message[sizeof(message) - 1] = '\0';
	output(message, rtlog_output_user_data);
}

u64 rtlog_now_ns(void) {
#if defined(_WIN32)
	static LARGE_INTEGER frequency;
	LARGE_INTEGER counter;
	if (frequency.QuadPart == 0) {
		QueryPerformanceFrequency(&frequency);
	}
	QueryPerformanceCounter(&counter);
	return (u64)((counter.QuadPart * 1000000000ull) / frequency.QuadPart);
#else
	struct timespec time_value;
	timespec_get(&time_value, TIME_UTC);
	return (u64)time_value.tv_sec * 1000000000ull + (u64)time_value.tv_nsec;
#endif
}

const char *rtlog_elapsed(u64 start_ns) {
	enum { RTLOG_ELAPSED_BUFFER_COUNT = 16 };
	enum { RTLOG_ELAPSED_BUFFER_SIZE = 64 };
	static RTLOG_THREAD_LOCAL char buffers[RTLOG_ELAPSED_BUFFER_COUNT][RTLOG_ELAPSED_BUFFER_SIZE];
	static RTLOG_THREAD_LOCAL u32 buffer_index = 0;
	char *buffer = buffers[buffer_index++ % RTLOG_ELAPSED_BUFFER_COUNT];
	u64 elapsed_ns = rtlog_now_ns() - start_ns;

	if (elapsed_ns >= 1000000ull) {
		snprintf(buffer, RTLOG_ELAPSED_BUFFER_SIZE, "%.3f ms", (f64)elapsed_ns / 1000000.0);
	} else if (elapsed_ns >= 1000ull) {
		snprintf(buffer, RTLOG_ELAPSED_BUFFER_SIZE, "%.3f us", (f64)elapsed_ns / 1000.0);
	} else {
		snprintf(buffer, RTLOG_ELAPSED_BUFFER_SIZE, "%llu ns", (u64)elapsed_ns);
	}
	buffer[RTLOG_ELAPSED_BUFFER_SIZE - 1] = '\0';
	return buffer;
}

void rtlog_call(const char *name) {
	rtlog_printf("%s()\n", name);
}

const char *rtlog_pointer(const void *pointer) {
	enum { RTLOG_POINTER_BUFFER_COUNT = 16 };
	enum { RTLOG_POINTER_BUFFER_SIZE = 2 + sizeof(void *) * 2 + 1 };
	static RTLOG_THREAD_LOCAL char buffers[RTLOG_POINTER_BUFFER_COUNT][RTLOG_POINTER_BUFFER_SIZE];
	static RTLOG_THREAD_LOCAL u32 buffer_index = 0;
	char *buffer = buffers[buffer_index++ % RTLOG_POINTER_BUFFER_COUNT];

	snprintf(buffer, RTLOG_POINTER_BUFFER_SIZE, "0x%0*llX", (i32)(sizeof(void *) * 2), (u64)(uptr)pointer);
	buffer[RTLOG_POINTER_BUFFER_SIZE - 1] = '\0';
	return buffer;
}

const char *rtlog_timepoint(rt_timepoint timepoint) {
	enum { RTLOG_TIMEPOINT_BUFFER_COUNT = 16 };
	enum { RTLOG_TIMEPOINT_BUFFER_SIZE = 128 };
	static RTLOG_THREAD_LOCAL char buffers[RTLOG_TIMEPOINT_BUFFER_COUNT][RTLOG_TIMEPOINT_BUFFER_SIZE];
	static RTLOG_THREAD_LOCAL u32 buffer_index = 0;
	char *buffer = buffers[buffer_index++ % RTLOG_TIMEPOINT_BUFFER_COUNT];

	snprintf(buffer, RTLOG_TIMEPOINT_BUFFER_SIZE, "{queue=%s, value=%llu}", rtlog_pointer(timepoint.queue), (u64)timepoint.value);
	buffer[RTLOG_TIMEPOINT_BUFFER_SIZE - 1] = '\0';
	return buffer;
}

void rtlog_error(const char *name) {
	enum rt_error error = next_rtError();

	if (error != RT_SUCCESS) {
		rtlog_printf("%s ERROR: %s\n", name, next_rtErrorMessage());
	}
}

#undef RTLOG_THREAD_LOCAL
