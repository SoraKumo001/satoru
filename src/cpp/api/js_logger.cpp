#include "api/js_logger.h"

#include <emscripten.h>

#include <cstdio>
#include <vector>

namespace satoru {

EM_JS(void, satoru_log_js, (int level, const char* message), {
    if (Module.onLog) {
        try {
            Module.onLog(level, UTF8ToString(message));
        } catch (e) {
            // Silently ignore log errors to prevent Wasm crash
        }
    }
});

JsLogger::JsLogger() : m_logLevel(LogLevel::None) {}

void JsLogger::setLogLevel(LogLevel level) { m_logLevel = level; }

LogLevel JsLogger::getLogLevel() const { return m_logLevel; }

void JsLogger::log(LogLevel level, const char* message) {
    if (level <= m_logLevel) {
        satoru_log_js(static_cast<int>(level), message);
    }
}

void JsLogger::logf(LogLevel level, const char* format, ...) {
    if (level <= m_logLevel) {
        va_list args;
        va_start(args, format);

        int size;
        {
            va_list args_copy;
            va_copy(args_copy, args);
            size = vsnprintf(nullptr, 0, format, args_copy);
            va_end(args_copy);
        }

        if (size >= 0) {
            std::vector<char> buffer(size + 1);
            vsnprintf(buffer.data(), buffer.size(), format, args);
            satoru_log_js(static_cast<int>(level), buffer.data());
        }

        va_end(args);
    }
}

}  // namespace satoru
