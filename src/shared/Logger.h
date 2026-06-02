#ifndef FIRELANDS_SHARED_LOGGER_H
#define FIRELANDS_SHARED_LOGGER_H

// Emoji labels for spdlog `%l` / `%L` (via level::to_string_view). Must precede any
// spdlog header so SPDLOG_LEVEL_NAMES is picked up by spdlog/common-inl.h.
#ifndef SPDLOG_LEVEL_NAMES
#define SPDLOG_LEVEL_NAMES                                                             \
  {                                                                                    \
    spdlog::string_view_t("\xf0\x9f\x94\x8d", 4),   /* trace 🔍 */                     \
    spdlog::string_view_t("\xf0\x9f\x90\x9b", 4),   /* debug 🐛 */                     \
    spdlog::string_view_t("\xf0\x9f\x92\xac", 4),   /* info 💬 */                      \
    spdlog::string_view_t("\xe2\x9a\xa0\xef\xb8\x8f", 6), /* warning ⚠️ */             \
    spdlog::string_view_t("\xe2\x9d\x8c", 3),       /* error ❌ */                     \
    spdlog::string_view_t("\xf0\x9f\x9a\xa8", 4),   /* critical 🚨 */                  \
    spdlog::string_view_t("-", 1)                   /* off */                          \
  }
#endif

#ifndef SPDLOG_SHORT_LEVEL_NAMES
#define SPDLOG_SHORT_LEVEL_NAMES                                                       \
  {                                                                                    \
    "\xf0\x9f\x94\x8d", "\xf0\x9f\x90\x9b", "\xf0\x9f\x92\xac", "\xe2\x9a\xa0\xef\xb8\x8f", \
    "\xe2\x9d\x8c", "\xf0\x9f\x9a\xa8", "-"                                            \
  }
#endif

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Firelands {

/**
 * @brief Logging level enumeration mapped to spdlog levels.
 *
 * Ordered from most verbose (Trace) to least verbose (Off).
 */
enum class LogLevel {
  Trace = SPDLOG_LEVEL_TRACE,
  Debug = SPDLOG_LEVEL_DEBUG,
  Info = SPDLOG_LEVEL_INFO,
  Warn = SPDLOG_LEVEL_WARN,
  Error = SPDLOG_LEVEL_ERROR,
  Critical = SPDLOG_LEVEL_CRITICAL,
  Off = SPDLOG_LEVEL_OFF
};

/**
 * @brief Configuration struct built by LoggerBuilder before Logger
 * initialization.
 *
 * Pattern tokens reference (spdlog/fmt):
 *   %H:%M:%S   — time (hour:min:sec)
 *   %e         — milliseconds
 *   %Y-%m-%d   — date
 *   %n         — logger name
 *   %l         — level text  (emoji tags: trace→🔍 debug→🐛 info→💬 warn→⚠️ err→❌ crit→🚨)
 *   %L         — short level (same emoji set via SPDLOG_SHORT_LEVEL_NAMES)
 *   %^  %$     — color range start / end (console only)
 *   %t         — thread id
 *   %v         — the actual log message
 *   %-Xl       — left-pad level to X characters
 */
struct LoggerConfig {
  std::string name = "firelands";
  bool enableConsole = true;
  bool enableFile = false;
  std::string filePath = "firelands.log";
  std::string mmapFilePath;
  std::size_t maxFileSizeBytes = 10 * 1024 * 1024; // 10 MB
  std::size_t maxFiles = 5;
  LogLevel consoleLevel = LogLevel::Info;
  LogLevel fileLevel = LogLevel::Debug;
  LogLevel mmapFileLevel = LogLevel::Debug;

  // Console: compact + colored level emoji, time only (no date noise)
  // Example: [23:18:34] [💬]  Starting Authentication Server...
  std::string consolePattern = "%^[%H:%M:%S] [%l]%$  %v";

  // File: full timestamp with ms, thread id, no ANSI codes (stays
  // grep-friendly) Example: [2026-04-11 23:18:34.042] [💬] [tid:1234]
  // Starting Authentication Server...
  std::string filePattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [tid:%t]  %v";
};

// ─────────────────────────────────────────────────────────────────────────────
// LoggerBuilder
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Fluent builder for LoggerConfig.
 *
 * Usage example:
 * @code
 *   Logger::Init(
 *       LoggerBuilder()
 *           .WithName("auth-server")
 *           .WithConsole(true)
 *           .WithFile(true, "logs/auth.log")
 *           .WithConsoleLevel(LogLevel::Info)
 *           .WithFileLevel(LogLevel::Debug)
 *           .Build()
 *   );
 * @endcode
 */
class LoggerBuilder {
public:
  LoggerBuilder &WithName(std::string name) {
    config_.name = std::move(name);
    return *this;
  }

  LoggerBuilder &WithConsole(bool enable) {
    config_.enableConsole = enable;
    return *this;
  }

  LoggerBuilder &WithFile(bool enable, std::string path = "firelands.log") {
    config_.enableFile = enable;
    config_.filePath = std::move(path);
    return *this;
  }

  LoggerBuilder &WithRotatingFile(std::size_t maxSizeBytes,
                                  std::size_t maxFiles) {
    config_.maxFileSizeBytes = maxSizeBytes;
    config_.maxFiles = maxFiles;
    return *this;
  }

  LoggerBuilder &WithConsoleLevel(LogLevel level) {
    config_.consoleLevel = level;
    return *this;
  }

  LoggerBuilder &WithFileLevel(LogLevel level) {
    config_.fileLevel = level;
    return *this;
  }

  LoggerBuilder &WithMmapFile(std::string path) {
    config_.mmapFilePath = std::move(path);
    return *this;
  }

  LoggerBuilder &WithMmapFileLevel(LogLevel level) {
    config_.mmapFileLevel = level;
    return *this;
  }

  /**
   * @brief Sets the pattern for the console sink only.
   *
   * Use spdlog pattern tokens. Color markers (%^ and %$) only take effect on
   * console.
   */
  LoggerBuilder &WithConsolePattern(std::string pattern) {
    config_.consolePattern = std::move(pattern);
    return *this;
  }

  /**
   * @brief Sets the pattern for the file sink only.
   *
   * Avoid color markers here — they produce raw ANSI escape sequences in the
   * log file.
   */
  LoggerBuilder &WithFilePattern(std::string pattern) {
    config_.filePattern = std::move(pattern);
    return *this;
  }

  /**
   * @brief Convenience: applies the same pattern to both console and file
   * sinks.
   */
  LoggerBuilder &WithPattern(std::string pattern) {
    config_.consolePattern = pattern;
    config_.filePattern = std::move(pattern);
    return *this;
  }

  [[nodiscard]] LoggerConfig Build() const { return config_; }

private:
  LoggerConfig config_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Logger
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Thread-safe Singleton Logger for the Firelands project.
 *
 * Wraps spdlog and supports simultaneous console and rotating-file output.
 * Must be initialized once at startup via Logger::Init() before any logging
 * call.
 *
 * Convenience macros are provided at the bottom of this header to enable
 * automatic source location capture (LOG_INFO, LOG_DEBUG, etc.).
 */
class Logger {
public:
  // ── Lifecycle ─────────────────────────────────────────────────────────

  /**
   * @brief Initializes the Singleton logger with the provided configuration.
   * @throws std::runtime_error if called more than once without a prior
   * Shutdown().
   */
  static void Init(const LoggerConfig &config = LoggerConfig{}) {
    if (instance_) {
      throw std::runtime_error("Logger::Init() called more than once. "
                               "Call Logger::Shutdown() first.");
    }
    instance_ = std::unique_ptr<Logger>(new Logger(config));
  }

  /**
   * @brief Flushes all sinks and resets the Singleton (useful for testing).
   */
  static void Shutdown() noexcept {
    if (instance_) {
      instance_->spdlogger_->flush();
    }
    instance_.reset();
    spdlog::shutdown();
  }

  /**
   * @brief Returns a reference to the active Logger instance.
   * @throws std::runtime_error if Logger::Init() has not been called.
   */
  static Logger &Get() {
    if (!instance_) {
      throw std::runtime_error(
          "Logger not initialized. Call Logger::Init() first.");
    }
    return *instance_;
  }

  /**
   * @brief Returns true when the Logger has been initialized.
   */
  static bool IsInitialized() noexcept { return instance_ != nullptr; }

  // ── Non-copyable, non-movable (Singleton) ─────────────────────────────
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

  // ── Log methods ───────────────────────────────────────────────────────

  template <typename... Args>
  void Trace(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlogger_->trace(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlogger_->debug(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlogger_->info(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Warn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlogger_->warn(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlogger_->error(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Critical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    spdlogger_->critical(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void MmapTrace(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    mmapSpdlogger_->trace(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void MmapDebug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    mmapSpdlogger_->debug(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void MmapInfo(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    mmapSpdlogger_->info(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void MmapWarn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    mmapSpdlogger_->warn(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void MmapError(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    mmapSpdlogger_->error(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void MmapCritical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    mmapSpdlogger_->critical(fmt, std::forward<Args>(args)...);
  }

  // ── Runtime configuration ─────────────────────────────────────────────

  /**
   * @brief Changes the minimum level for all sinks at runtime.
   */
  void SetLevel(LogLevel level) noexcept {
    spdlogger_->set_level(static_cast<spdlog::level::level_enum>(level));
  }

  /**
   * @brief Flushes all pending log entries to their respective sinks.
   */
  void Flush() noexcept { spdlogger_->flush(); }

  /**
   * @brief Returns the underlying spdlog logger (advanced use only).
   */
  [[nodiscard]] std::shared_ptr<spdlog::logger> GetSpdLogger() const noexcept {
    return spdlogger_;
  }

  [[nodiscard]] std::shared_ptr<spdlog::logger> GetMmapSpdLogger() const
      noexcept {
    return mmapSpdlogger_;
  }

  /// Console sink pattern from the active `LoggerConfig` (for TUI mirroring).
  [[nodiscard]] const std::string &GetConsolePattern() const noexcept {
    return console_pattern_;
  }

private:
  explicit Logger(const LoggerConfig &config)
      : console_pattern_(config.consolePattern) {
    std::vector<spdlog::sink_ptr> sinks;

    if (config.enableConsole) {
      auto consoleSink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      consoleSink->set_level(
          static_cast<spdlog::level::level_enum>(config.consoleLevel));
      consoleSink->set_pattern(config.consolePattern);
      sinks.push_back(std::move(consoleSink));
    }

    if (config.enableFile) {
      auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          config.filePath, config.maxFileSizeBytes, config.maxFiles);
      fileSink->set_level(
          static_cast<spdlog::level::level_enum>(config.fileLevel));
      fileSink->set_pattern(config.filePattern);
      sinks.push_back(std::move(fileSink));
    }

    if (sinks.empty()) {
      // Guarantee at least one sink to avoid silent drops.
      auto nullSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      nullSink->set_level(spdlog::level::off);
      sinks.push_back(std::move(nullSink));
    }

    spdlogger_ = std::make_shared<spdlog::logger>(config.name, sinks.begin(),
                                                  sinks.end());

    // The logger-level must be the minimum of all sinks so messages reach them.
    spdlogger_->set_level(spdlog::level::trace);
    spdlogger_->flush_on(spdlog::level::err);

    spdlog::register_logger(spdlogger_);

    std::string mmapFilePath = config.mmapFilePath;
    if (mmapFilePath.empty()) {
      std::filesystem::path basePath(config.filePath);
      if (basePath.has_filename()) {
        std::filesystem::path mmapPath = basePath;
        mmapPath.replace_filename(basePath.stem().string() + "-mmaps" +
                                   basePath.extension().string());
        mmapFilePath = mmapPath.string();
      } else {
        mmapFilePath = "logs/firelands-mmaps.log";
      }
    }

    auto mmapSink =
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(mmapFilePath);
    mmapSink->set_level(
        static_cast<spdlog::level::level_enum>(config.mmapFileLevel));
    mmapSink->set_pattern(config.filePattern);

    mmapSpdlogger_ = std::make_shared<spdlog::logger>(config.name + ".mmap",
                                                      mmapSink);
    mmapSpdlogger_->set_level(spdlog::level::trace);
    mmapSpdlogger_->flush_on(spdlog::level::err);
    spdlog::register_logger(mmapSpdlogger_);
  }

  std::shared_ptr<spdlog::logger> spdlogger_;
  std::shared_ptr<spdlog::logger> mmapSpdlogger_;
  std::string console_pattern_;
  static std::unique_ptr<Logger> instance_;
};

// Static member definition
inline std::unique_ptr<Logger> Logger::instance_ = nullptr;

} // namespace Firelands

// ─────────────────────────────────────────────────────────────────────────────
// Convenience macros (use these in production code)
// They forward to the global Singleton and preserve call-site readability.
// ─────────────────────────────────────────────────────────────────────────────
#define LOG_TRACE(...) ::Firelands::Logger::Get().Trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::Firelands::Logger::Get().Debug(__VA_ARGS__)
#define LOG_INFO(...) ::Firelands::Logger::Get().Info(__VA_ARGS__)
#define LOG_WARN(...) ::Firelands::Logger::Get().Warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Firelands::Logger::Get().Error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::Firelands::Logger::Get().Critical(__VA_ARGS__)
#define LOG_MMAP_TRACE(...) ::Firelands::Logger::Get().MmapTrace(__VA_ARGS__)
#define LOG_MMAP_DEBUG(...) ::Firelands::Logger::Get().MmapDebug(__VA_ARGS__)
#define LOG_MMAP_INFO(...) ::Firelands::Logger::Get().MmapInfo(__VA_ARGS__)
#define LOG_MMAP_WARN(...) ::Firelands::Logger::Get().MmapWarn(__VA_ARGS__)
#define LOG_MMAP_ERROR(...) ::Firelands::Logger::Get().MmapError(__VA_ARGS__)
#define LOG_MMAP_CRITICAL(...) ::Firelands::Logger::Get().MmapCritical(__VA_ARGS__)

#endif // FIRELANDS_SHARED_LOGGER_H
