#ifndef SATORU_UTILS_LOGGING_H
#define SATORU_UTILS_LOGGING_H

#include "../bridge/bridge_types.h"

// Define macros for easier logging.
// These call direct satoru_log_printf call (printf-style).

#ifdef __cplusplus
extern "C" {
#endif
void satoru_log_printf(LogLevel level, const char* format, ...);
#ifdef __cplusplus
}
#endif

#define SATORU_LOG_DEBUG(...) satoru_log_printf(LogLevel::Debug, __VA_ARGS__)

#define SATORU_LOG_INFO(...) satoru_log_printf(LogLevel::Info, __VA_ARGS__)

#define SATORU_LOG_WARN(...) satoru_log_printf(LogLevel::Warning, __VA_ARGS__)

#define SATORU_LOG_ERROR(...) satoru_log_printf(LogLevel::Error, __VA_ARGS__)

#endif  // SATORU_UTILS_LOGGING_H
