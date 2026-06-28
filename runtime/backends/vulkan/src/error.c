#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/


static thread_local enum rt_error rtvk_error_status = RT_SUCCESS;
static thread_local char rtvk_error_text[1024] = "";

static thread_local PFN_rtOutput rtvk_output = NULL;
static thread_local void* rtvk_output_user_data = NULL;

static void rtvk_default_output(const char* message, void* user_data) {
	fputs(message, stdout);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtSetOutput(PFN_rtOutput output, void* user_data) {
	rtvk_output = output;
	rtvk_output_user_data = user_data;
}

void rtvk_vprintf(const char* format, va_list args) {
	char message[1024];
	PFN_rtOutput output = rtvk_output ? rtvk_output : rtvk_default_output;

	if (!format) { return; }

	vsnprintf(message, sizeof(message), format, args);
	message[sizeof(message) - 1] = '\0';
	output(message, rtvk_output_user_data);
}

void rtvk_printf(const char* format, ...) {
	va_list args;
	va_start(args, format);
	rtvk_vprintf(format, args);
	va_end(args);
}

void rtvk_throwf(enum rt_error error, const char* format, ...) {
	va_list args;

	rtvk_error_status = error;
	if (!format) {
		rtvk_error_text[0] = '\0';
		return;
	}

	va_start(args, format);
	vsnprintf(rtvk_error_text, sizeof(rtvk_error_text), format, args);
	va_end(args);
	rtvk_error_text[sizeof(rtvk_error_text) - 1] = '\0';
}

enum rt_error rtvk_error_from_vk(VkResult result) {
	switch (result) {
	case VK_ERROR_OUT_OF_HOST_MEMORY: return RT_OUT_OF_HOST_MEMORY;
	case VK_ERROR_OUT_OF_DEVICE_MEMORY:
		return RT_OUT_OF_DEVICE_MEMORY;
	case VK_ERROR_INITIALIZATION_FAILED:
		return RT_INITIALIZATION_FAILED;
	case VK_ERROR_LAYER_NOT_PRESENT:
		return RT_LAYER_NOT_PRESENT;
	case VK_ERROR_EXTENSION_NOT_PRESENT:
		return RT_EXTENSION_NOT_PRESENT;
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return RT_INCOMPATIBLE_DRIVER;
	default:
		return RT_INITIALIZATION_FAILED;
	}
}

enum rt_error rtError(void) {
	return rtvk_error();
}

enum rt_error rtvk_error(void) {
	return rtvk_error_status;
}

const char* rtErrorMessage(void) {
	return rtvk_error_text;
}

void rtClearError(void) {
	rtvk_error_status = RT_SUCCESS;
	rtvk_error_text[0] = '\0';
}
