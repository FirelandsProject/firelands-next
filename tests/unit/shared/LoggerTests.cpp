#include <gtest/gtest.h>
#include <shared/Logger.h>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Firelands;
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Test fixture: guarantees the Singleton is clean before/after each test.
// ─────────────────────────────────────────────────────────────────────────────
class LoggerTest : public ::testing::Test {
protected:
    void TearDown() override {
        if (Logger::IsInitialized()) {
            Logger::Shutdown();
        }
        // Clean up temp log files written during tests
        if (fs::exists(kTestLogPath_)) {
            fs::remove(kTestLogPath_);
        }
    }

    static constexpr const char* kTestLogPath_ = "test_firelands.log";
};

// ─────────────────────────────────────────────────────────────────────────────
// 1. Initialization
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(LoggerTest, InitializesSuccessfully) {
    EXPECT_FALSE(Logger::IsInitialized());
    EXPECT_NO_THROW(Logger::Init());
    EXPECT_TRUE(Logger::IsInitialized());
}

TEST_F(LoggerTest, DoubleInitThrows) {
    Logger::Init();
    EXPECT_THROW(Logger::Init(), std::runtime_error);
}

TEST_F(LoggerTest, GetBeforeInitThrows) {
    EXPECT_THROW(Logger::Get(), std::runtime_error);
}

TEST_F(LoggerTest, ShutdownAllowsReinit) {
    Logger::Init();
    EXPECT_NO_THROW(Logger::Shutdown());
    EXPECT_FALSE(Logger::IsInitialized());
    EXPECT_NO_THROW(Logger::Init());
    EXPECT_TRUE(Logger::IsInitialized());
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. LoggerBuilder
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(LoggerTest, BuilderConsoleOnlyConfig) {
    auto config = LoggerBuilder()
        .WithName("test-console")
        .WithConsole(true)
        .WithFile(false)
        .WithConsoleLevel(LogLevel::Warn)
        .Build();

    EXPECT_EQ(config.name, "test-console");
    EXPECT_TRUE(config.enableConsole);
    EXPECT_FALSE(config.enableFile);
    EXPECT_EQ(config.consoleLevel, LogLevel::Warn);
}

TEST_F(LoggerTest, BuilderFileConfig) {
    auto config = LoggerBuilder()
        .WithName("test-file")
        .WithConsole(false)
        .WithFile(true, kTestLogPath_)
        .WithFileLevel(LogLevel::Debug)
        .WithRotatingFile(5 * 1024 * 1024, 3)
        .Build();

    EXPECT_FALSE(config.enableConsole);
    EXPECT_TRUE(config.enableFile);
    EXPECT_EQ(config.filePath, kTestLogPath_);
    EXPECT_EQ(config.fileLevel, LogLevel::Debug);
    EXPECT_EQ(config.maxFileSizeBytes, 5u * 1024u * 1024u);
    EXPECT_EQ(config.maxFiles, 3u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. File output
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(LoggerTest, WritesToFileWhenEnabled) {
    auto config = LoggerBuilder()
        .WithName("file-logger")
        .WithConsole(false)
        .WithFile(true, kTestLogPath_)
        .WithFileLevel(LogLevel::Trace)
        .Build();

    Logger::Init(config);
    LOG_INFO("Hello from file logger test");
    Logger::Get().Flush();

    EXPECT_TRUE(fs::exists(kTestLogPath_));

    std::ifstream logFile(kTestLogPath_);
    std::string   content((std::istreambuf_iterator<char>(logFile)),
                           std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Hello from file logger test") != std::string::npos);
}

TEST_F(LoggerTest, DoesNotCreateFileWhenDisabled) {
    auto config = LoggerBuilder()
        .WithName("console-only")
        .WithConsole(true)
        .WithFile(false)
        .Build();

    Logger::Init(config);
    LOG_INFO("This should not go to a file");
    Logger::Get().Flush();

    EXPECT_FALSE(fs::exists(kTestLogPath_));
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. All log levels through macros
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(LoggerTest, AllLogLevelMacrosCompileAndRun) {
    auto config = LoggerBuilder()
        .WithName("levels-test")
        .WithConsole(false)
        .WithFile(true, kTestLogPath_)
        .WithFileLevel(LogLevel::Trace)
        .Build();

    Logger::Init(config);

    EXPECT_NO_THROW({
        LOG_TRACE("Trace message: {}", 1);
        LOG_DEBUG("Debug message: {}", 2);
        LOG_INFO("Info  message: {}", 3);
        LOG_WARN("Warn  message: {}", 4);
        LOG_ERROR("Error message: {}", 5);
        LOG_CRITICAL("Critical message: {}", 6);
    });

    Logger::Get().Flush();
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Runtime level change
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(LoggerTest, SetLevelFiltersMessages) {
    auto config = LoggerBuilder()
        .WithName("level-change")
        .WithConsole(false)
        .WithFile(true, kTestLogPath_)
        .WithFileLevel(LogLevel::Trace)
        .Build();

    Logger::Init(config);

    // Raise level to Error — Debug messages should be dropped.
    Logger::Get().SetLevel(LogLevel::Error);
    LOG_DEBUG("This debug line must NOT appear");
    LOG_ERROR("This error line MUST appear");
    Logger::Get().Flush();

    std::ifstream logFile(kTestLogPath_);
    std::string   content((std::istreambuf_iterator<char>(logFile)),
                           std::istreambuf_iterator<char>());

    EXPECT_EQ(content.find("This debug line must NOT appear"), std::string::npos);
    EXPECT_NE(content.find("This error line MUST appear"),    std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Formatted output
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(LoggerTest, FormattedMessagesAreWrittenCorrectly) {
    auto config = LoggerBuilder()
        .WithName("fmt-test")
        .WithConsole(false)
        .WithFile(true, kTestLogPath_)
        .WithFileLevel(LogLevel::Trace)
        .Build();

    Logger::Init(config);
    LOG_INFO("Player {} connected from {}", "Arthas", "127.0.0.1");
    Logger::Get().Flush();

    std::ifstream logFile(kTestLogPath_);
    std::string   content((std::istreambuf_iterator<char>(logFile)),
                           std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("Arthas"), std::string::npos);
    EXPECT_NE(content.find("127.0.0.1"), std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Separate console / file patterns
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(LoggerTest, SeparatePatternsApplyPerSink) {
    // Use a plain pattern for file (no color tokens) so we can verify the
    // exact text that ends up on disk without ANSI escape sequences.
    auto config = LoggerBuilder()
        .WithName("pattern-test")
        .WithConsole(false)
        .WithFile(true, kTestLogPath_)
        .WithFileLevel(LogLevel::Trace)
        .WithFilePattern("[%H:%M:%S] [%l] %v")
        .Build();

    Logger::Init(config);
    LOG_INFO("PatternCheck");
    Logger::Get().Flush();

    std::ifstream logFile(kTestLogPath_);
    std::string   content((std::istreambuf_iterator<char>(logFile)),
                           std::istreambuf_iterator<char>());

    // Message must appear
    EXPECT_NE(content.find("PatternCheck"), std::string::npos);
    // No ANSI color escape sequences in the file
    EXPECT_EQ(content.find("\033["), std::string::npos);
}

TEST_F(LoggerTest, WithPatternSetsBoth) {
    auto config = LoggerBuilder()
        .WithName("both-pattern")
        .WithConsole(true)
        .WithFile(true, kTestLogPath_)
        .WithFileLevel(LogLevel::Trace)
        .WithPattern("%v")  // bare message, applies to both sinks
        .Build();

    EXPECT_EQ(config.consolePattern, "%v");
    EXPECT_EQ(config.filePattern,    "%v");
}
