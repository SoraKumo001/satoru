#ifndef SATORU_CORE_NULL_LOGGER_H
#define SATORU_CORE_NULL_LOGGER_H

#include "core/ilogger.h"

namespace satoru {

class NullLogger : public ILogger {
   public:
    void log(LogLevel /*level*/, const char* /*message*/) override {}
    void logf(LogLevel /*level*/, const char* /*format*/, ...) override {}
};

}  // namespace satoru

#endif  // SATORU_CORE_NULL_LOGGER_H
