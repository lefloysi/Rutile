#include "core.hpp"

#include "context.hpp"
#include "error.hpp"

#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static bool rtdx_feature_equals(const char* feature, const char* expected) {
	return feature && strcmp(feature, expected) == 0;
}

static bool rtdx_validate_init_features(const char* const* features, u32 feature_count, rtdx_context_flags* flags) {
	if (feature_count && !features) {
		rtdx_throwf(RT_IMPROPER_USAGE, "rtInit feature_count is %u but features is NULL", feature_count);
		return false;
	}

	*flags = {};
	for (u32 i = 0; i < feature_count; i++) {
		const char* feature = features[i];
		if (!feature) {
			rtdx_throwf(RT_IMPROPER_USAGE, "rtInit feature at index %u is NULL", i);
			return false;
		}
		if (rtdx_feature_equals(feature, RT_FEATURE_PRESENTATION)) {
			flags->presentation = true;
			continue;
		}

		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "unsupported rtInit feature: %s", feature);
		return false;
	}

	return true;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtInit(const char* const* features, u32 feature_count) {
	rtdx_context_flags flags;

	rtClearError();
	if (current_context) {
		rtdx_throwf(RT_ALREADY_INITIALIZED, "rtInit called while rt-directx12 is already initialized");
		return;
	}

	if (!rtdx_validate_init_features(features, feature_count, &flags)) {
		return;
	}

	current_context = rtdx_create_context(flags);
}

void rtExit(void) {
	rtdx_context_destroy(current_context);
	current_context = NULL;
}
