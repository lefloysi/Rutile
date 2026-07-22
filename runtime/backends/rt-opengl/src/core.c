#include "core.h"
#include "context.h"

#include <stdio.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef struct rtgl_setting_entry {
	char backend_name[64];
	char value[256];
} rtgl_setting_entry;

static rtgl_setting_entry rtgl_settings[64];
static u32 rtgl_setting_count = 0;
static bool rtgl_force_version = false;
static u08 rtgl_force_major = 0;
static u08 rtgl_force_minor = 0;

static bool rtgl_backend_equals(const char* backend_name) {
	return backend_name && strcmp(backend_name, "rt-opengl") == 0;
}

RTGL_API void rtInit(const char* const* features, u32 feature_count) {
	rtgl_context_flags flags;

	rtClearError();
	if (current_context) {
		rtgl_throwf(RT_ALREADY_INITIALIZED, "rtInit called while rt-opengl is already initialized");
		return;
	}

	if (feature_count && !features) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtInit feature_count is %u but features is NULL", feature_count);
		return;
	}

	flags = (rtgl_context_flags){ 0 };
	for (u32 i = 0; i < feature_count; i++) {
		const char* feature = features[i];
		if (!feature) {
			rtgl_throwf(RT_IMPROPER_USAGE, "rtInit feature at index %u is NULL", i);
			return;
		}
		if (strcmp(feature, RT_FEATURE_PRESENTATION) == 0) {
			flags.presentation = true;
			continue;
		}
		rtgl_throwf(RT_UNSUPPORTED_FEATURE, "unsupported rtInit feature: %s", feature);
		return;
	}

	current_context = rtgl_create_context(flags);
}

RTGL_API void rtExit(void) {
	rtgl_context_destroy(current_context);
	current_context = NULL;
}

RTGL_API void rtSettingApply(const char* backend_name, const char* value) {
	unsigned major = 0;
	unsigned minor = 0;

	if (!backend_name || !value) {
		return;
	}

	if (rtgl_setting_count < (u32)(sizeof(rtgl_settings) / sizeof(rtgl_settings[0]))) {
		snprintf(rtgl_settings[rtgl_setting_count].backend_name, sizeof(rtgl_settings[rtgl_setting_count].backend_name), "%s", backend_name);
		rtgl_settings[rtgl_setting_count].backend_name[sizeof(rtgl_settings[rtgl_setting_count].backend_name) - 1] = '\0';
		snprintf(rtgl_settings[rtgl_setting_count].value, sizeof(rtgl_settings[rtgl_setting_count].value), "%s", value);
		rtgl_settings[rtgl_setting_count].value[sizeof(rtgl_settings[rtgl_setting_count].value) - 1] = '\0';
		rtgl_setting_count++;
	}

	if (!rtgl_backend_equals(backend_name)) {
		return;
	}

	if (sscanf(value, "-v %u.%u", &major, &minor) == 2 || sscanf(value, "--version %u.%u", &major, &minor) == 2) {
		if (major <= 255 && minor <= 255) {
			rtgl_force_version = true;
			rtgl_force_major = (u08)major;
			rtgl_force_minor = (u08)minor;
		}
	}
}

RTGL_API const char* rtGetName(void) {
	return "rt-opengl";
}

RTGL_API enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	if (format == RT_RGBA8_UNORM) {
		return RT_FORMAT_USAGE_SAMPLED |
			   RT_FORMAT_USAGE_COLOR_ATTACHMENT |
			   RT_FORMAT_USAGE_TRANSFER_SRC |
			   RT_FORMAT_USAGE_TRANSFER_DST;
	}
	return RT_FORMAT_USAGE_NONE;
}

bool rtgl_forced_context_version(u08* major, u08* minor) {
	if (!rtgl_force_version) {
		return false;
	}
	if (major) {
		*major = rtgl_force_major;
	}
	if (minor) {
		*minor = rtgl_force_minor;
	}
	return true;
}
