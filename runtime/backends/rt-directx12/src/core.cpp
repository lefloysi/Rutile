#include "core.hpp"

#include <cstdio>
#include <cstring>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtSettingApply(const char* name, const char* value) {
	(void)name;
	(void)value;
}

const char* rtGetName(void) { return "rt-directx12"; }

enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	return RT_FORMAT_USAGE_NONE;
}
