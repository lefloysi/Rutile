#include "core.h"
#include "context.h"

#include <stdio.h>
#include <string.h>

static bool rtvk_validate_init_features(const char* const* features, u32 feature_count, rtvk_context_flags* flags);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef struct rtvk_setting_entry {
	char backend_name[64];
	char value[256];
} rtvk_setting_entry;

static rtvk_setting_entry rtvk_settings[64];
static u32 rtvk_setting_count = 0;

static bool rtvk_backend_equals(const char* backend_name) {
	return backend_name && strcmp(backend_name, "rt-vulkan") == 0;
}

void rtInit(const char* const* features, u32 feature_count) {
	rtvk_context_flags flags;

	rtClearError();
	if (current_context) {
		rtvk_throwf(RT_ALREADY_INITIALIZED, "rtInit called while rt-vulkan is already initialized");
		return;
	}

	if (!rtvk_validate_init_features(features, feature_count, &flags)) {
		return;
	}

	current_context = rtvk_create_context(flags);
}
void rtExit(void) {
	rtvk_context_destroy(current_context);
	current_context = NULL;
}
void rtSettingApply(const char* backend_name, const char* value) {
	if (!backend_name || !value) {
		return;
	}
	if (rtvk_setting_count < (u32)(sizeof(rtvk_settings) / sizeof(rtvk_settings[0]))) {
		snprintf(rtvk_settings[rtvk_setting_count].backend_name, sizeof(rtvk_settings[rtvk_setting_count].backend_name), "%s", backend_name);
		rtvk_settings[rtvk_setting_count].backend_name[sizeof(rtvk_settings[rtvk_setting_count].backend_name) - 1] = '\0';
		snprintf(rtvk_settings[rtvk_setting_count].value, sizeof(rtvk_settings[rtvk_setting_count].value), "%s", value);
		rtvk_settings[rtvk_setting_count].value[sizeof(rtvk_settings[rtvk_setting_count].value) - 1] = '\0';
		rtvk_setting_count++;
	}

	if (!rtvk_backend_equals(backend_name)) {
		return;
	}
}
const char* rtGetName(void) { return "rt-vulkan"; }
enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	struct rtvk_context* ctx = rtvk_get_current_context();
	if (!ctx || !ctx->vk_physical_device) {
		return RT_FORMAT_USAGE_NONE;
	}

	VkFormat vk_format = rtvk_format_to_vk(format);
	if (vk_format == VK_FORMAT_UNDEFINED) {
		return RT_FORMAT_USAGE_NONE;
	}

	VkFormatProperties properties;
	vkGetPhysicalDeviceFormatProperties(ctx->vk_physical_device, vk_format, &properties);
	return rtvk_usage_from_vk_features(properties.optimalTilingFeatures);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

VkFormat rtvk_format_to_vk(enum rt_format format) {
	switch (format) {
	case RT_R8_UNORM:
		return VK_FORMAT_R8_UNORM;
	case RT_RG8_UNORM:
		return VK_FORMAT_R8G8_UNORM;
	case RT_RGB8_UNORM:
		return VK_FORMAT_R8G8B8_UNORM;
	case RT_RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case RT_R16_UNORM:
		return VK_FORMAT_R16_UNORM;
	case RT_RG16_UNORM:
		return VK_FORMAT_R16G16_UNORM;
	case RT_RGB16_UNORM:
		return VK_FORMAT_R16G16B16_UNORM;
	case RT_RGBA16_UNORM:
		return VK_FORMAT_R16G16B16A16_UNORM;
	case RT_R16_SFLOAT:
		return VK_FORMAT_R16_SFLOAT;
	case RT_RG16_SFLOAT:
		return VK_FORMAT_R16G16_SFLOAT;
	case RT_RGB16_SFLOAT:
		return VK_FORMAT_R16G16B16_SFLOAT;
	case RT_RGBA16_SFLOAT:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case RT_R32_SFLOAT:
		return VK_FORMAT_R32_SFLOAT;
	case RT_RG32_SFLOAT:
		return VK_FORMAT_R32G32_SFLOAT;
	case RT_RGB32_SFLOAT:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case RT_RGBA32_SFLOAT:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case RT_R8_SINT:
		return VK_FORMAT_R8_SINT;
	case RT_RG8_SINT:
		return VK_FORMAT_R8G8_SINT;
	case RT_RGB8_SINT:
		return VK_FORMAT_R8G8B8_SINT;
	case RT_RGBA8_SINT:
		return VK_FORMAT_R8G8B8A8_SINT;
	case RT_R16_SINT:
		return VK_FORMAT_R16_SINT;
	case RT_RG16_SINT:
		return VK_FORMAT_R16G16_SINT;
	case RT_RGB16_SINT:
		return VK_FORMAT_R16G16B16_SINT;
	case RT_RGBA16_SINT:
		return VK_FORMAT_R16G16B16A16_SINT;
	case RT_R32_SINT:
		return VK_FORMAT_R32_SINT;
	case RT_RG32_SINT:
		return VK_FORMAT_R32G32_SINT;
	case RT_RGB32_SINT:
		return VK_FORMAT_R32G32B32_SINT;
	case RT_RGBA32_SINT:
		return VK_FORMAT_R32G32B32A32_SINT;
	case RT_R8_UINT:
		return VK_FORMAT_R8_UINT;
	case RT_RG8_UINT:
		return VK_FORMAT_R8G8_UINT;
	case RT_RGB8_UINT:
		return VK_FORMAT_R8G8B8_UINT;
	case RT_RGBA8_UINT:
		return VK_FORMAT_R8G8B8A8_UINT;
	case RT_R16_UINT:
		return VK_FORMAT_R16_UINT;
	case RT_RG16_UINT:
		return VK_FORMAT_R16G16_UINT;
	case RT_RGB16_UINT:
		return VK_FORMAT_R16G16B16_UINT;
	case RT_RGBA16_UINT:
		return VK_FORMAT_R16G16B16A16_UINT;
	case RT_R32_UINT:
		return VK_FORMAT_R32_UINT;
	case RT_RG32_UINT:
		return VK_FORMAT_R32G32_UINT;
	case RT_RGB32_UINT:
		return VK_FORMAT_R32G32B32_UINT;
	case RT_RGBA32_UINT:
		return VK_FORMAT_R32G32B32A32_UINT;
	case RT_D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case RT_D32_SFLOAT:
		return VK_FORMAT_D32_SFLOAT;
	case RT_S8_UINT:
		return VK_FORMAT_S8_UINT;
	case RT_D24_UNORM_S8_UINT:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case RT_D32_SFLOAT_S8_UINT:
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}
enum rt_format_usage rtvk_usage_from_vk_features(VkFormatFeatureFlags features) {
	enum rt_format_usage usage = RT_FORMAT_USAGE_NONE;
	if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
		usage |= RT_FORMAT_USAGE_SAMPLED;
	}
	if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
		usage |= RT_FORMAT_USAGE_COLOR_ATTACHMENT;
	}
	if (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		usage |= RT_FORMAT_USAGE_DEPTH_ATTACHMENT;
	}
	if (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
		usage |= RT_FORMAT_USAGE_STORAGE;
	}
	if (features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
		usage |= RT_FORMAT_USAGE_TRANSFER_SRC;
	}
	if (features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
		usage |= RT_FORMAT_USAGE_TRANSFER_DST;
	}
	return usage;
}
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
