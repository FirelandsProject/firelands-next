#include <shared/tui/FtxuiLogSpdlog.h>

#include <shared/Logger.h>
#include <shared/tui/FtxuiLogSink.h>

#include <stdexcept>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#endif

namespace Firelands {
namespace {

bool IsTerminalStdoutSink(spdlog::sink_ptr const &s) {
  return std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(s) ||
         std::dynamic_pointer_cast<spdlog::sinks::stdout_sink_mt>(s) ||
         std::dynamic_pointer_cast<spdlog::sinks::stderr_color_sink_mt>(s)
#ifdef _WIN32
         || std::dynamic_pointer_cast<spdlog::sinks::wincolor_stdout_sink_mt>(s)
#endif
      ;
}

} // namespace

void BindFirelandsLoggerToFtxuiSink(FtxuiLogSinkPtr const &sink) {
  if (!sink) {
    throw std::invalid_argument("BindFirelandsLoggerToFtxuiSink: sink is null");
  }
  if (!Logger::IsInitialized()) {
    throw std::runtime_error(
        "BindFirelandsLoggerToFtxuiSink: Logger::Init() required first");
  }
  sink->set_pattern(Logger::Get().GetConsolePattern());
  ReplaceStdoutColorSinkWith(sink);
}

void ReplaceStdoutColorSinkWith(spdlog::sink_ptr replacement) {
  auto lg = Logger::Get().GetSpdLogger();
  auto &sinks = lg->sinks();
  spdlog::level::level_enum lvl = spdlog::level::info;
  for (auto it = sinks.begin(); it != sinks.end();) {
    if (IsTerminalStdoutSink(*it)) {
      lvl = (*it)->level();
      it = sinks.erase(it);
    } else {
      ++it;
    }
  }
  replacement->set_level(lvl);
  sinks.insert(sinks.begin(), std::move(replacement));
}

void RestoreStdoutColorSink(spdlog::sink_ptr ftxui_sink) {
  auto lg = Logger::Get().GetSpdLogger();
  auto &sinks = lg->sinks();
  for (auto it = sinks.begin(); it != sinks.end();) {
    if (*it == ftxui_sink) {
      it = sinks.erase(it);
    } else {
      ++it;
    }
  }
  auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console->set_level(ftxui_sink->level());
  if (Logger::IsInitialized()) {
    console->set_pattern(Logger::Get().GetConsolePattern());
  } else {
    console->set_pattern("%^[%H:%M:%S] [%l]%$  %v");
  }
  sinks.insert(sinks.begin(), console);
}

void MuteTerminalLogSinks() {
  auto lg = Logger::Get().GetSpdLogger();
  for (auto const &s : lg->sinks()) {
    if (IsTerminalStdoutSink(s)) {
      s->set_level(spdlog::level::off);
    }
  }
}

} // namespace Firelands
