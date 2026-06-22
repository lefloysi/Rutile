#include "core.h"

#include "context.h"
#include "error.h"

#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

/*
** SPEC.html §5.2 Initialization, §5.3 Shutdown
** Implements rtInit and rtExit plus feature-string validation.
** rtExit is a no-op when initialization never succeeded.
*/

static bool rtvk_feature_equals(const char* feature, const char* expected) {
	return feature && strcmp(feature, expected) == 0;
}

static bool rtvk_validate_init_features(const char* const* features, u32 feature_count, rtvk_context_flags* flags) {
	if (feature_count && !features) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtInit feature_count is %u but features is NULL", feature_count);
		return false;
	}

	*flags = (rtvk_context_flags){ 0 };
	for (u32 i = 0; i < feature_count; i++) {
		const char* feature = features[i];
		if (!feature) {
			rtvk_throwf(RT_IMPROPER_USAGE, "rtInit feature at index %u is NULL", i);
			return false;
		}
		if (rtvk_feature_equals(feature, RT_FEATURE_PRESENTATION)) {
			flags->presentation = true;
			continue;
		}
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "unsupported rtInit feature: %s", feature);
		return false;
	}

	return true;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtInit(const char* const* features, u32 feature_count) {
	rtvk_context_flags flags;

	rtClearError();
	if (current_context) {
		rtvk_throwf(RT_ALREADY_INITIALIZED, "rtInit called while rt-vulkan is already initialized");
		return;
	}

	if (!rtvk_validate_init_features(features, feature_count, &flags)) { return; }

	current_context = rtvk_create_context(flags);
}

void rtExit(void) {
	rtvk_context_destroy(current_context);
	current_context = NULL;
}


