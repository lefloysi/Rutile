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

	if (!format) {
		return;
	}

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
		/* Fall back to the rt_error name so callers that pass NULL still
		 * leave something useful for rtErrorMessage() to return. */
		snprintf(
			rtvk_error_text,
			sizeof(rtvk_error_text),
			"%s (no further detail)",
			rtvk_rt_error_name(error)
		);
		rtvk_error_text[sizeof(rtvk_error_text) - 1] = '\0';
		return;
	}

	va_start(args, format);
	vsnprintf(rtvk_error_text, sizeof(rtvk_error_text), format, args);
	va_end(args);
	rtvk_error_text[sizeof(rtvk_error_text) - 1] = '\0';
}

enum rt_error rtvk_error_from_vk(VkResult result) {
	switch (result) {
	case VK_ERROR_OUT_OF_HOST_MEMORY:
		return RT_OUT_OF_HOST_MEMORY;
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

const char* rtvk_vk_result_name(VkResult result) {
	switch (result) {
	case VK_SUCCESS:                                            return "VK_SUCCESS";
	case VK_NOT_READY:                                          return "VK_NOT_READY";
	case VK_TIMEOUT:                                            return "VK_TIMEOUT";
	case VK_EVENT_SET:                                          return "VK_EVENT_SET";
	case VK_EVENT_RESET:                                        return "VK_EVENT_RESET";
	case VK_INCOMPLETE:                                         return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY:                           return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY:                         return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED:                        return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST:                                  return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED:                            return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT:                            return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT:                        return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT:                          return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER:                          return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS:                             return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED:                         return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL:                              return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN:                                      return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY:                           return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE:                      return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION:                                return "VK_ERROR_FRAGMENTATION";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:               return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_ERROR_SURFACE_LOST_KHR:                             return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:                     return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR:                                     return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR:                              return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:                     return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_VALIDATION_FAILED_EXT:                        return "VK_ERROR_VALIDATION_FAILED_EXT";
	case VK_ERROR_INVALID_SHADER_NV:                            return "VK_ERROR_INVALID_SHADER_NV";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:          return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	default:                                                    return "VK_ERROR_<unknown>";
	}
}

const char* rtvk_rt_error_name(enum rt_error error) {
	switch ((int)error) {
	case RT_SUCCESS:                  return "RT_SUCCESS";
	case RT_OUT_OF_HOST_MEMORY:       return "RT_OUT_OF_HOST_MEMORY";
	case RT_OUT_OF_DEVICE_MEMORY:     return "RT_OUT_OF_DEVICE_MEMORY";
	case RT_IMPROPER_USAGE:           return "RT_IMPROPER_USAGE";
	case RT_PLATFORM_FAILURE:         return "RT_PLATFORM_FAILURE";
	case RT_DEVICE_LOST:              return "RT_DEVICE_LOST";
	case RT_ALREADY_INITIALIZED:      return "RT_ALREADY_INITIALIZED";
	case RT_UNSUPPORTED_PLATFORM:     return "RT_UNSUPPORTED_PLATFORM";
	case RT_NO_BACKEND:               return "RT_NO_BACKEND";
	case RT_UNSUPPORTED_FEATURE:      return "RT_UNSUPPORTED_FEATURE";
	case RT_INITIALIZATION_FAILED:    return "RT_INITIALIZATION_FAILED";
	case RT_LAYER_NOT_PRESENT:        return "RT_LAYER_NOT_PRESENT";
	case RT_EXTENSION_NOT_PRESENT:    return "RT_EXTENSION_NOT_PRESENT";
	case RT_INCOMPATIBLE_DRIVER:      return "RT_INCOMPATIBLE_DRIVER";
	case RT_SHADER_COMPILATION_FAILED:return "RT_SHADER_COMPILATION_FAILED";
	case RT_SHADER_LINK_FAILED:       return "RT_SHADER_LINK_FAILED";
	case RT_FEATURE_NOT_SUPPORTED:    return "RT_FEATURE_NOT_SUPPORTED";
	default:                          return "RT_ERROR_<unknown>";
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
