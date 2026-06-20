#include "error.h"

#include <stdio.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static thread_local enum rt_error rtdx_error = RT_SUCCESS;
static thread_local char rtdx_error_text[1024] = "";

static PFN_rtOutput rtdx_output = NULL;
static void* rtdx_output_user_data = NULL;

static const char* rtdx_hresult_fallback(HRESULT result) {
	switch (result) {
	case E_OUTOFMEMORY: return "E_OUTOFMEMORY";
	case DXGI_ERROR_DEVICE_REMOVED: return "DXGI_ERROR_DEVICE_REMOVED";
	case DXGI_ERROR_DEVICE_RESET: return "DXGI_ERROR_DEVICE_RESET";
	case DXGI_ERROR_DEVICE_HUNG: return "DXGI_ERROR_DEVICE_HUNG";
	case DXGI_ERROR_UNSUPPORTED: return "DXGI_ERROR_UNSUPPORTED";
	case DXGI_ERROR_INVALID_CALL: return "DXGI_ERROR_INVALID_CALL";
	default: return "HRESULT";
	}
}

static void rtdx_default_output(const char* message, void* user_data) {
	fputs(message, stdout);
	fflush(stdout);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtSetOutput(PFN_rtOutput output, void* user_data) {
	rtdx_output = output;
	rtdx_output_user_data = user_data;
}

void rtdx_vprintf(const char* format, va_list args) {
	char message[1024];
	PFN_rtOutput output = rtdx_output ? rtdx_output : rtdx_default_output;

	if (!format) { return; }

	vsnprintf(message, sizeof(message), format, args);
	message[sizeof(message) - 1] = '\0';
	output(message, rtdx_output_user_data);
}

void rtdx_printf(const char* format, ...) {
	va_list args;
	va_start(args, format);
	rtdx_vprintf(format, args);
	va_end(args);
}

void rtdx_throwf(enum rt_error error, const char* format, ...) {
	va_list args;

	rtdx_error = error;
	if (!format) {
		rtdx_error_text[0] = '\0';
		return;
	}

	va_start(args, format);
	vsnprintf(rtdx_error_text, sizeof(rtdx_error_text), format, args);
	va_end(args);
	rtdx_error_text[sizeof(rtdx_error_text) - 1] = '\0';
}

enum rt_error rtdx_error_from_hresult(HRESULT result) {
	switch (result) {
	case E_OUTOFMEMORY: return RT_OUT_OF_HOST_MEMORY;
	case DXGI_ERROR_DEVICE_REMOVED:
	case DXGI_ERROR_DEVICE_RESET:
	case DXGI_ERROR_DEVICE_HUNG: return RT_DEVICE_LOST;
	case DXGI_ERROR_UNSUPPORTED: return RT_UNSUPPORTED_PLATFORM;
	default: return RT_INITIALIZATION_FAILED;
	}
}

const char* rtdx_hresult_name(HRESULT result) {
	static thread_local char text[64];
	const char* name = rtdx_hresult_fallback(result);
	if (name != "HRESULT") {
		return name;
	}
	snprintf(text, sizeof(text), "HRESULT(0x%08x)", (u32)result);
	text[sizeof(text) - 1] = '\0';
	return text;
}

enum rt_error rtError(void) {
	return rtdx_error;
}

const char* rtErrorMessage(void) {
	return rtdx_error_text;
}

void rtClearError(void) {
	rtdx_error = RT_SUCCESS;
	rtdx_error_text[0] = '\0';
}


