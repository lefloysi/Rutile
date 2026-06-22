#include "error.h"

#include <stdio.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static thread_local enum rt_error rtgl_error_status = RT_SUCCESS;
static thread_local char rtgl_error_text[1024] = "";

static thread_local PFN_rtOutput rtgl_output = NULL;
static thread_local void* rtgl_output_user_data = NULL;

static void rtgl_default_output(const char* message, void*) {
	fputs(message, stdout);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtSetOutput(PFN_rtOutput output, void* user_data) {
	rtgl_set_output(output, user_data);
}

enum rt_error rtError(void) {
	return rtgl_error();
}

const char* rtErrorMessage(void) {
	return rtgl_error_message();
}

void rtClearError(void) {
	rtgl_clear_error();
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtgl_vprintf(const char* format, va_list args) {
	char message[1024];
	PFN_rtOutput output = rtgl_output ? rtgl_output : rtgl_default_output;

	if (!format) {
		return;
	}

	vsnprintf(message, sizeof(message), format, args);
	message[sizeof(message) - 1] = '\0';
	output(message, rtgl_output_user_data);
}

void rtgl_printf(const char* format, ...) {
	va_list args;
	va_start(args, format);
	rtgl_vprintf(format, args);
	va_end(args);
}

void rtgl_throwf(enum rt_error error, const char* format, ...) {
	va_list args;

	rtgl_error_status = error;
	if (!format) {
		rtgl_error_text[0] = '\0';
		return;
	}

	va_start(args, format);
	vsnprintf(rtgl_error_text, sizeof(rtgl_error_text), format, args);
	va_end(args);
	rtgl_error_text[sizeof(rtgl_error_text) - 1] = '\0';
}

void rtgl_set_output(PFN_rtOutput output, void* user_data) {
	rtgl_output = output;
	rtgl_output_user_data = user_data;
}

void rtgl_clear_error(void) {
	rtgl_error_status = RT_SUCCESS;
	rtgl_error_text[0] = '\0';
}

enum rt_error rtgl_error(void) {
	return rtgl_error_status;
}

const char* rtgl_error_message(void) {
	return rtgl_error_text;
}
