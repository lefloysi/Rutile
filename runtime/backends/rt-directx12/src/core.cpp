#include "core.hpp"

#include <cstdio>
#include <cstring>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_setting_entry {
	char backend_name[64];
	char value[256];
};

static rtdx_setting_entry rtdx_settings[64];
static u32 rtdx_setting_count = 0;

static bool rtdx_backend_equals(const char* backend_name) {
	return backend_name && std::strcmp(backend_name, "rt-directx12") == 0;
}

void rtSettingApply(const char* backend_name, const char* value) {
	if (!backend_name || !value) {
		return;
	}
	if (rtdx_setting_count < (u32)(sizeof(rtdx_settings) / sizeof(rtdx_settings[0]))) {
		std::snprintf(rtdx_settings[rtdx_setting_count].backend_name, sizeof(rtdx_settings[rtdx_setting_count].backend_name), "%s", backend_name);
		rtdx_settings[rtdx_setting_count].backend_name[sizeof(rtdx_settings[rtdx_setting_count].backend_name) - 1] = '\0';
		std::snprintf(rtdx_settings[rtdx_setting_count].value, sizeof(rtdx_settings[rtdx_setting_count].value), "%s", value);
		rtdx_settings[rtdx_setting_count].value[sizeof(rtdx_settings[rtdx_setting_count].value) - 1] = '\0';
		rtdx_setting_count++;
	}

	if (!rtdx_backend_equals(backend_name)) {
		return;
	}
}

const char* rtGetName(void) { return "rt-directx12"; }

enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	return RT_FORMAT_USAGE_NONE;
}
