#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT void rtInit(const char *const *features, u32 feature_count) { rtlog_rtInit(features, feature_count); }
RT_EXPORT void rtExit(void) { rtlog_rtExit(); }
RT_EXPORT void rtSetOutput(PFN_rtOutput output, void *user_data) { rtlog_rtSetOutput(output, user_data); }
RT_EXPORT enum rt_error rtError(void) { return rtlog_rtError(); }
RT_EXPORT const char *rtErrorMessage(void) { return rtlog_rtErrorMessage(); }
RT_EXPORT void rtClearError(void) { rtlog_rtClearError(); }
RT_EXPORT const char *rtGetName(void) { return rtlog_rtGetName(); }
RT_EXPORT enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) { return rtlog_rtQueryFormatCapabilities(format); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtlog_rtInit(const char *const *features, u32 feature_count) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtInit(features=%s, feature_count=%u)\n", rtlog_pointer(features), feature_count);
	next_rtInit(features, feature_count);
	rtlog_printf("rtInit completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtInit");
}

void rtlog_rtExit(void) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtExit()\n");
	next_rtExit();
	rtlog_printf("rtExit completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtExit");
}

void rtlog_rtSetOutput(PFN_rtOutput output, void *user_data) {
	rtlog_set_output(output, user_data);
	next_rtSetOutput(output, user_data);
}

enum rt_error rtlog_rtError(void) {
	return next_rtError();
}

const char *rtlog_rtErrorMessage(void) {
	return next_rtErrorMessage();
}

void rtlog_rtClearError(void) {
	next_rtClearError();
}

const char *rtlog_rtGetName(void) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtGetName()\n");
	const char *result = next_rtGetName();
	rtlog_printf("rtGetName -> \"%s\" [%s]\n", result ? result : "", rtlog_elapsed(start_ns));
	rtlog_error("rtGetName");
	return result;
}

enum rt_format_usage rtlog_rtQueryFormatCapabilities(enum rt_format format) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtQueryFormatCapabilities(format=%d)\n", (i32)format);
	enum rt_format_usage result = next_rtQueryFormatCapabilities(format);
	rtlog_printf("rtQueryFormatCapabilities -> %d [%s]\n", (i32)result, rtlog_elapsed(start_ns));
	rtlog_error("rtQueryFormatCapabilities");
	return result;
}
