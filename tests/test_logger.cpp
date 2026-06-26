#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "core/ilogger.h"
#include "core/null_logger.h"
#include "utils/logging.h"

// --- Test Logger Implementation ---

class TestLogger : public satoru::ILogger {
   public:
    struct LogEntry {
        LogLevel level;
        std::string message;
    };

    std::vector<LogEntry> entries;
    LogLevel min_level = LogLevel::Debug;

    void log(LogLevel level, const char* message) override {
        if (level != LogLevel::None && level <= min_level) {
            entries.push_back({level, message ? message : ""});
        }
    }

    void logf(LogLevel level, const char* format, ...) override {
        if (level != LogLevel::None && level <= min_level) {
            char buffer[512];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            entries.push_back({level, std::string(buffer)});
        }
    }

    void clear() { entries.clear(); }

    bool has_message(const std::string& msg) const {
        for (const auto& entry : entries) {
            if (entry.message.find(msg) != std::string::npos) return true;
        }
        return false;
    }

    bool has_level(LogLevel level) const {
        for (const auto& entry : entries) {
            if (entry.level == level) return true;
        }
        return false;
    }
};

// --- ILogger Interface Tests ---

TEST(ILoggerTest, NullLoggerDoesNotCrash) {
    satoru::NullLogger logger;
    // Should not crash or throw
    logger.log(LogLevel::Info, "test message");
    logger.log(LogLevel::Error, nullptr);
    logger.logf(LogLevel::Debug, "format %s %d", "test", 42);
}

TEST(ILoggerTest, TestLoggerCapturesLog) {
    TestLogger logger;
    logger.log(LogLevel::Info, "Hello World");
    ASSERT_EQ(logger.entries.size(), 1u);
    EXPECT_EQ(logger.entries[0].level, LogLevel::Info);
    EXPECT_EQ(logger.entries[0].message, "Hello World");
}

TEST(ILoggerTest, TestLoggerCapturesLogf) {
    TestLogger logger;
    logger.logf(LogLevel::Error, "Error code: %d", 404);
    ASSERT_EQ(logger.entries.size(), 1u);
    EXPECT_EQ(logger.entries[0].level, LogLevel::Error);
    EXPECT_EQ(logger.entries[0].message, "Error code: 404");
}

TEST(ILoggerTest, TestLoggerFiltersByLevel) {
    TestLogger logger;
    logger.min_level = LogLevel::Warning;

    logger.log(LogLevel::Debug, "debug");
    logger.log(LogLevel::Info, "info");
    logger.log(LogLevel::Warning, "warn");
    logger.log(LogLevel::Error, "error");

    ASSERT_EQ(logger.entries.size(), 2u);
    EXPECT_FALSE(logger.has_message("debug"));
    EXPECT_FALSE(logger.has_message("info"));
    EXPECT_TRUE(logger.has_message("warn"));
    EXPECT_TRUE(logger.has_message("error"));
}

TEST(ILoggerTest, TestLoggerClear) {
    TestLogger logger;
    logger.log(LogLevel::Info, "test");
    ASSERT_EQ(logger.entries.size(), 1u);
    logger.clear();
    ASSERT_EQ(logger.entries.size(), 0u);
}

// --- Global Logger Accessor Tests ---

class GlobalLoggerTest : public ::testing::Test {
   protected:
    satoru::NullLogger fallback;

    void SetUp() override {
        // Reset to null so fallback is used
        satoru_log_set_logger(nullptr);
    }

    void TearDown() override {
        satoru_log_set_logger(nullptr);
    }
};

TEST_F(GlobalLoggerTest, DefaultReturnsFallback) {
    auto* logger = satoru_log_get_logger();
    ASSERT_NE(logger, nullptr);
    // Should not crash - it's a NullLogger
    logger->log(LogLevel::Info, "test");
}

TEST_F(GlobalLoggerTest, SetAndGetLogger) {
    TestLogger test_logger;
    satoru_log_set_logger(&test_logger);

    auto* logger = satoru_log_get_logger();
    ASSERT_EQ(logger, &test_logger);

    logger->log(LogLevel::Info, "through global");
    ASSERT_EQ(test_logger.entries.size(), 1u);
    EXPECT_EQ(test_logger.entries[0].message, "through global");
}

TEST_F(GlobalLoggerTest, SetNullReturnsFallback) {
    TestLogger test_logger;
    satoru_log_set_logger(&test_logger);
    satoru_log_set_logger(nullptr);

    auto* logger = satoru_log_get_logger();
    ASSERT_NE(logger, &test_logger);
    // Should be the fallback NullLogger
    logger->log(LogLevel::Info, "test");
    EXPECT_TRUE(test_logger.entries.empty());
}

// --- SATORU_LOG_* Macro Tests ---

class LoggingMacroTest : public ::testing::Test {
   protected:
    TestLogger test_logger;

    void SetUp() override { satoru_log_set_logger(&test_logger); }

    void TearDown() override { satoru_log_set_logger(nullptr); }
};

TEST_F(LoggingMacroTest, SatoruLogInfo) {
    SATORU_LOG_INFO("Info message %d", 123);
    ASSERT_EQ(test_logger.entries.size(), 1u);
    EXPECT_EQ(test_logger.entries[0].level, LogLevel::Info);
    EXPECT_EQ(test_logger.entries[0].message, "Info message 123");
}

TEST_F(LoggingMacroTest, SatoruLogWarn) {
  SATORU_LOG_WARN("Warning!");
  ASSERT_EQ(test_logger.entries.size(), 1u);
  EXPECT_EQ(test_logger.entries[0].level, LogLevel::Warning);
  EXPECT_EQ(test_logger.entries[0].message, "Warning!");
}

TEST_F(LoggingMacroTest, SatoruLogError) {
    SATORU_LOG_ERROR("Error: %s", "fail");
    ASSERT_EQ(test_logger.entries.size(), 1u);
    EXPECT_EQ(test_logger.entries[0].level, LogLevel::Error);
    EXPECT_EQ(test_logger.entries[0].message, "Error: fail");
}

TEST_F(LoggingMacroTest, SatoruLogDebug) {
    SATORU_LOG_DEBUG("Debug info");
    ASSERT_EQ(test_logger.entries.size(), 1u);
    EXPECT_EQ(test_logger.entries[0].level, LogLevel::Debug);
    EXPECT_EQ(test_logger.entries[0].message, "Debug info");
}
