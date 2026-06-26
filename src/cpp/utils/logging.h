#ifndef SATORU_UTILS_LOGGING_H
#define SATORU_UTILS_LOGGING_H

#include "../bridge/bridge_types.h"
#include "../core/ilogger.h"

// Legacy macros for backward compatibility.
// New code should use ILogger directly via SatoruContext::getLogger().

#define SATORU_LOG_DEBUG(...)                                     \
    do {                                                          \
        auto* _logger = satoru_log_get_logger();                  \
        if (_logger) _logger->logf(LogLevel::Debug, __VA_ARGS__); \
    } while (0)

#define SATORU_LOG_INFO(...)                                     \
    do {                                                         \
        auto* _logger = satoru_log_get_logger();                 \
        if (_logger) _logger->logf(LogLevel::Info, __VA_ARGS__); \
    } while (0)

#define SATORU_LOG_WARN(...)                                        \
    do {                                                            \
        auto* _logger = satoru_log_get_logger();                    \
        if (_logger) _logger->logf(LogLevel::Warning, __VA_ARGS__); \
    } while (0)

#define SATORU_LOG_ERROR(...)                                     \
    do {                                                          \
        auto* _logger = satoru_log_get_logger();                  \
        if (_logger) _logger->logf(LogLevel::Error, __VA_ARGS__); \
    } while (0)

// Global logger access for legacy macros.
// In production, set via satoru_log_set_logger().
// In tests, set to NullLogger.
#ifdef __cplusplus
extern "C" {
#endif
satoru::ILogger* satoru_log_get_logger();
void satoru_log_set_logger(satoru::ILogger* logger);
#ifdef __cplusplus
}
#endif

#endif  // SATORU_UTILS_LOGGING_H
