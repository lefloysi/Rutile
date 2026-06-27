#include "logger.h"
#include "procs.h"

#include <stdio.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static PFN_rtOutput rtval_output = NULL;
static void *rtval_output_user_data = NULL;

static void rtval_default_output(const char *message, void *user_data) {
	fputs(message, stdout);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvalSetOutput(PFN_rtOutput output, void *user_data) {
	rtval_output = output;
	rtval_output_user_data = user_data;
}

void rtval_vprintf(const char *format, va_list args) {
	char message[1024];
	PFN_rtOutput output = rtval_output ? rtval_output : rtval_default_output;

	if (!format) {
		return;
	}

	vsnprintf(message, sizeof(message), format, args);
	message[sizeof(message) - 1] = '\0';
	output(message, rtval_output_user_data);
}

void rtval_printf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	rtval_vprintf(format, args);
	va_end(args);
}

void rtval_report_error(const char *call_name) {
	enum rt_error error = rtval_next_rtError();
	if (error == RT_SUCCESS) {
		return;
	}

	const char *message = rtval_next_rtErrorMessage();
	rtval_printf("[validation] %s failed: error=%d message=\"%s\"\n", call_name ? call_name : "<unknown>", (i32)error, message ? message : "");
	rtval_next_rtClearError();
}
