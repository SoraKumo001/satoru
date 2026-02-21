#ifndef SATORU_UTILS_LOGGING_H
#define SATORU_UTILS_LOGGING_H

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include "../bridge/bridge_types.h"

// Define macros for easier logging.
// These use spdlog's "satoru" logger if it exists, otherwise fall back to direct satoru_log call.

#define SATORU_LOG_DEBUG(...)                                              \
    do {                                                                   \
        if (auto l = spdlog::get("satoru"))                                \
            l->debug(__VA_ARGS__);                                         \
        else                                                               \
            satoru_log(LogLevel::Debug, fmt::format(__VA_ARGS__).c_str()); \
    } while (0)

#define SATORU_LOG_INFO(...)                                              \
    do {                                                                  \
        if (auto l = spdlog::get("satoru"))                               \
            l->info(__VA_ARGS__);                                         \
        else                                                              \
            satoru_log(LogLevel::Info, fmt::format(__VA_ARGS__).c_str()); \
    } while (0)

#define SATORU_LOG_WARN(...)                                                 \
    do {                                                                     \
        if (auto l = spdlog::get("satoru"))                                  \
            l->warn(__VA_ARGS__);                                            \
        else                                                                 \
            satoru_log(LogLevel::Warning, fmt::format(__VA_ARGS__).c_str()); \
    } while (0)

#define SATORU_LOG_ERROR(...)                                              \
    do {                                                                   \
        if (auto l = spdlog::get("satoru"))                                \
            l->error(__VA_ARGS__);                                         \
        else                                                               \
            satoru_log(LogLevel::Error, fmt::format(__VA_ARGS__).c_str()); \
    } while (0)

#endif  // SATORU_UTILS_LOGGING_H
