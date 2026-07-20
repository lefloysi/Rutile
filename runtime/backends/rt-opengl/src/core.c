#include "core.h"
#include "context.h"

#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

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

RTGL_API const char* rtGetName(void) {
	return "rt-opengl";
}

RTGL_API enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	(void)format;
	return RT_FORMAT_USAGE_NONE;
}
