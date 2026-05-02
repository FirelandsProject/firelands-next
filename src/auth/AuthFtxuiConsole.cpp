#include "AuthFtxuiConsole.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <shared/Logger.h>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#endif

#include <atomic>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Firelands {
namespace {

using namespace ftxui;

std::string StripTerminalAnsi(std::string const &in) {
  std::string out;
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size();) {
    if (in[i] == '\x1b' && i + 1 < in.size() && in[i + 1] == '[') {
      i += 2;
      while (i < in.size() &&
             !std::isalpha(static_cast<unsigned char>(in[i]))) {
        ++i;
      }
      if (i < in.size()) {
        ++i;
      }
      continue;
    }
    out.push_back(in[i++]);
  }
  return out;
}

class FtxuiLogSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit FtxuiLogSink(std::size_t maxLines) : max_lines_(maxLines) {
    set_pattern("[%H:%M:%S] [%l]  %v");
  }

  bool ConsumeRenderDirty() {
    return dirty_.exchange(false, std::memory_order_acq_rel);
  }

  std::vector<std::string> CopyRecentLines(std::size_t maxRender) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (lines_.size() <= maxRender) {
      return std::vector<std::string>(lines_.begin(), lines_.end());
    }
    return std::vector<std::string>(lines_.end() - maxRender, lines_.end());
  }

protected:
  void sink_it_(const spdlog::details::log_msg &msg) override {
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string line;
    line.append(formatted.data(), formatted.size());
    lines_.push_back(std::move(line));
    while (lines_.size() > max_lines_) {
      lines_.pop_front();
    }
    dirty_.store(true, std::memory_order_release);
  }

  void flush_() override {}

private:
  std::size_t max_lines_;
  std::deque<std::string> lines_;
  std::atomic<bool> dirty_{false};
};

void ReplaceStdoutColorSinkWith(spdlog::sink_ptr replacement) {
  auto lg = Logger::Get().GetSpdLogger();
  auto &sinks = lg->sinks();
  spdlog::level::level_enum lvl = spdlog::level::info;
  for (auto it = sinks.begin(); it != sinks.end();) {
    auto &s = *it;
    if (std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(s) ||
        std::dynamic_pointer_cast<spdlog::sinks::stdout_sink_mt>(s) ||
        std::dynamic_pointer_cast<spdlog::sinks::stderr_color_sink_mt>(s)
#ifdef _WIN32
        || std::dynamic_pointer_cast<spdlog::sinks::wincolor_stdout_sink_mt>(s)
#endif
    ) {
      lvl = (*it)->level();
      it = sinks.erase(it);
    } else {
      ++it;
    }
  }
  replacement->set_level(lvl);
  sinks.insert(sinks.begin(), std::move(replacement));
}

/// Same FIRELANDS block + caption as `PrintBanner(BannerType::Auth)` (Banner.h).
Element AuthTuiBanner() {
  static constexpr char const *const kBlockLogo[] = {
      "    ███████╗██╗██████╗ ███████╗██╗      █████╗ ███╗   ██╗██████╗ ███████╗",
      "    ██╔════╝██║██╔══██╗██╔════╝██║     ██╔══██╗████╗  ██║██╔══██╗██╔════╝",
      "    █████╗  ██║██████╔╝█████╗  ██║     ███████║██╔██╗ ██║██║  ██║███████╗",
      "    ██╔══╝  ██║██╔══██╗██╔══╝  ██║     ██╔══██║██║╚██╗██║██║  ██║╚════██║",
      "    ██║     ██║██║  ██║███████╗███████╗██║  ██║██║ ╚████║██████╔╝███████║",
      "    ╚═╝     ╚═╝╚═╝  ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═════╝ ╚══════╝",
  };
  Color const rowColors[] = {
      Color::RGB(110, 38, 32),  Color::RGB(145, 48, 36), Color::RGB(185, 62, 40),
      Color::RGB(220, 85, 48),  Color::RGB(255, 118, 60), Color::RGB(255, 175, 130),
  };
  Elements logoRows;
  logoRows.reserve(6);
  for (int i = 0; i < 6; ++i) {
    logoRows.push_back(text(kBlockLogo[i]) | bold | color(rowColors[i]));
  }

  Element const caption = center(hbox({
      text("Cataclysm WoW Emulator | ") | color(Color::RGB(232, 228, 220)),
      text("AUTH SERVER") | bold | color(Color::RGB(100, 205, 248)),
      text(" | Build 15595") | color(Color::RGB(232, 228, 220)),
  }));

  Element const rule =
      center(text(std::string(72, '-'))) | color(Color::RGB(95, 88, 82));

  return vbox({
             center(vbox(std::move(logoRows))),
             text(" ") | size(HEIGHT, EQUAL, 1),
             caption,
             rule,
         }) |
         bgcolor(Color::RGB(22, 20, 18)) |
         borderStyled(ROUNDED, Color::RGB(72, 64, 58));
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
  console->set_pattern("%^[%H:%M:%S] [%l]%$  %v");
  sinks.insert(sinks.begin(), console);
}

void MuteTerminalLogSinks() {
  auto lg = Logger::Get().GetSpdLogger();
  for (auto const &s : lg->sinks()) {
    if (std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(s) ||
        std::dynamic_pointer_cast<spdlog::sinks::stdout_sink_mt>(s) ||
        std::dynamic_pointer_cast<spdlog::sinks::stderr_color_sink_mt>(s)
#ifdef _WIN32
        || std::dynamic_pointer_cast<spdlog::sinks::wincolor_stdout_sink_mt>(s)
#endif
    ) {
      s->set_level(spdlog::level::off);
    }
  }
}

} // namespace (anonymous)

using namespace ftxui;

void RunAuthFtxuiConsole(AsyncNetworkServer &authTcpServer,
                         AsyncNetworkServer *realmLinkTcpServer) {
  auto screen = ScreenInteractive::Fullscreen();
  Closure const requestExit = screen.ExitLoopClosure();

  std::shared_ptr<FtxuiLogSink> log_sink =
      std::make_shared<FtxuiLogSink>(std::size_t(12000));
  ReplaceStdoutColorSinkWith(log_sink);

  auto key_sink = Container::Vertical({});
  key_sink |= CatchEvent([requestExit](Event const &e) {
    if (!e.is_character() || e.character().size() != 1) {
      return false;
    }
    char const c = e.character()[0];
    if (c == 'q' || c == 'Q') {
      requestExit();
      return true;
    }
    return false;
  });

  Color const kAccent = Color::RGB(255, 118, 60);
  Color const kLogBg = Color::RGB(48, 44, 42);
  Color const kLogFg = Color::RGB(235, 232, 225);
  Color const kShellBg = Color::RGB(28, 26, 24);

  auto root = Renderer(key_sink, [&] {
    Dimensions const term = Terminal::Size();
    int const term_h = (term.dimy > 2) ? term.dimy : 25;
    int const kBannerBudget = 12;
    int const kBottomChrome = 3;
    int const log_h =
        std::max(6, term_h - kBannerBudget - 1 - kBottomChrome);

    constexpr std::size_t kBufferLines = 4000;
    std::vector<std::string> lines = log_sink->CopyRecentLines(kBufferLines);
    if (lines.size() > static_cast<std::size_t>(log_h)) {
      lines.assign(lines.end() - static_cast<std::ptrdiff_t>(log_h),
                   lines.end());
    }

    Elements rows;
    rows.reserve(lines.size());
    for (auto const &ln : lines) {
      rows.push_back(text(StripTerminalAnsi(ln)) | color(kLogFg));
    }
    if (rows.empty()) {
      rows.push_back(text("(waiting for log output...)") | color(Color::GrayLight));
    }

    Element const log_body = vbox(std::move(rows)) | bgcolor(kLogBg);
    Element const log_title = hbox({
        text(" ") | bgcolor(kAccent),
        text(" log ") | bold | color(Color::RGB(210, 200, 190)),
        filler() | bgcolor(Color::RGB(40, 36, 34)),
    });
    Element const log_area =
        window(log_title, log_body) | size(HEIGHT, EQUAL, log_h);

    Element const footer = hbox({
        text(" ") | bgcolor(kAccent),
        text("  Q  quit  ") | bold | color(Color::RGB(248, 242, 232)),
        text("  ·  Ctrl+C  stop  ") | dim | color(Color::RGB(180, 170, 160)),
        filler() | bgcolor(Color::RGB(36, 34, 32)),
    });

    Element const banner = AuthTuiBanner();

    return vbox({
               banner | notflex,
               separator() | color(Color::RGB(110, 100, 92)),
               log_area,
               filler() | flex,
               separator() | color(Color::RGB(110, 100, 92)),
               footer,
           }) |
           bgcolor(kShellBg);
  });

  std::atomic<bool> run_ticks{true};
  std::thread ticker([&] {
    while (run_ticks.load()) {
      authTcpServer.Update();
      if (realmLinkTcpServer != nullptr) {
        realmLinkTcpServer->Update();
      }
      if (log_sink->ConsumeRenderDirty()) {
        screen.Post(Event::Custom);
        screen.RequestAnimationFrame();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  screen.Loop(root);
  run_ticks = false;
  if (ticker.joinable()) {
    ticker.join();
  }

  RestoreStdoutColorSink(log_sink);
  MuteTerminalLogSinks();
}

} // namespace Firelands
