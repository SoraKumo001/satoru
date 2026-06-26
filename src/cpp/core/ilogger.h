#ifndef SATORU_CORE_ILOGGER_H
#define SATORU_CORE_ILOGGER_H

#include <cstdarg>

#include "bridge/bridge_types.h"

namespace satoru {

class ILogger {
   public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level, const char* message) = 0;
    virtual void logf(LogLevel level, const char* format, ...) = 0;
};

}  // namespace satoru

#endif  // SATORU_CORE_ILOGGER_H
