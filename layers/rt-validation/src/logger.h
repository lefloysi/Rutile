#ifndef RTVAL_LOGGER_H
#define RTVAL_LOGGER_H

#include "types.h"

#include <stdarg.h>

void rtvalSetOutput(PFN_rtOutput output, void* user_data);
void rtval_printf(const char* format, ...);
void rtval_vprintf(const char* format, va_list args);
void rtval_report_error(const char* call_name);

#endif


