#include "utils/logging.h"

#include "core/ilogger.h"
#include "core/null_logger.h"

namespace {
satoru::ILogger* g_logger = nullptr;
satoru::NullLogger g_null_logger;
}  // namespace

satoru::ILogger* satoru_log_get_logger() { return g_logger ? g_logger : &g_null_logger; }

void satoru_log_set_logger(satoru::ILogger* logger) { g_logger = logger; }
