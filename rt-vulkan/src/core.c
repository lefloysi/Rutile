#include "core.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

const char* rtGetName(void) { return "rt-vulkan"; }
enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	const enum rt_format_usage transfer =
		RT_FORMAT_USAGE_TRANSFER_SRC |
		RT_FORMAT_USAGE_TRANSFER_DST;

	switch (format) {
	case RT_RGBA8_UNORM:
		return RT_FORMAT_USAGE_SAMPLED |
			RT_FORMAT_USAGE_COLOR_ATTACHMENT |
			RT_FORMAT_USAGE_STORAGE |
			transfer;
	case RT_D16_UNORM:
	case RT_D32_SFLOAT:
	case RT_S8_UINT:
	case RT_D24_UNORM_S8_UINT:
	case RT_D32_SFLOAT_S8_UINT:
		return RT_FORMAT_USAGE_SAMPLED |
			RT_FORMAT_USAGE_DEPTH_ATTACHMENT |
			transfer;
	default:
		return RT_FORMAT_USAGE_NONE;
	}
}


