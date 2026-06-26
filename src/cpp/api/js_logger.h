#ifndef SATORU_API_JS_LOGGER_H
#define SATORU_API_JS_LOGGER_H

#include <cstdarg>

#include "core/ilogger.h"

namespace satoru {

class JsLogger : public ILogger {
   public:
    JsLogger();
    ~JsLogger() override = default;

    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    void log(LogLevel level, const char* message) override;
    void logf(LogLevel level, const char* format, ...) override;

   private:
    LogLevel m_logLevel;
};

}  // namespace satoru

#endif  // SATORU_API_JS_LOGGER_H
