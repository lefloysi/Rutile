#include "procs.h"
#include "logger.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT void rtInit(const char* const* features, u32 feature_count) { rtval_rtInit(features, feature_count); }
RT_EXPORT void rtExit(void) { rtval_rtExit(); }
RT_EXPORT enum rt_error rtError(void) { return rtval_rtError(); }
RT_EXPORT const char* rtErrorMessage(void) { return rtval_rtErrorMessage(); }
RT_EXPORT void rtClearError(void) { rtval_rtClearError(); }
RT_EXPORT const char* rtGetName(void) { return rtval_rtGetName(); }
RT_EXPORT enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) { return rtval_rtQueryFormatCapabilities(format); }
RT_EXPORT void rtSetOutput(PFN_rtOutput output, void* user_data) { rtval_rtSetOutput(output, user_data); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtval_rtInit(const char* const* features, u32 feature_count) {
	rtval_next_rtInit(features, feature_count);
	rtval_report_error("rtInit");
}

void rtval_rtExit(void) {
	rtval_command_buffer_reset_tracking();
	rtval_next_rtExit();
	rtval_report_error("rtExit");
}

enum rt_error rtval_rtError(void) {
	return rtval_next_rtError();
}

const char* rtval_rtErrorMessage(void) {
	return rtval_next_rtErrorMessage();
}

void rtval_rtClearError(void) {
	rtval_next_rtClearError();
}

const char* rtval_rtGetName(void) {
	return rtval_next_rtGetName();
}

enum rt_format_usage rtval_rtQueryFormatCapabilities(enum rt_format format) {
	enum rt_format_usage usage = rtval_next_rtQueryFormatCapabilities(format);
	rtval_report_error("rtQueryFormatCapabilities");
	return usage;
}

void rtval_rtSetOutput(PFN_rtOutput output, void* user_data) {
	rtvalSetOutput(output, user_data);
	rtval_next_rtSetOutput(output, user_data);
}


