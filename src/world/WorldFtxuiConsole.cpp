#include "WorldFtxuiConsole.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "WorldInteractiveConsole.h"
#include <infrastructure/network/asio/AsyncNetworkServer.h>
#include <shared/Logger.h>
#include <shared/system/SystemClipboard.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <deque>
#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace Firelands {
namespace {

using namespace ftxui;

/// spdlog file sink is plain; console may still emit CSI via future patterns.
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

// Must match `WorldTuiBanner()` (6 + spacer + caption + rule) + ROUNDED border (+2).
int constexpr kWorldBannerScreenRows = 11;
int constexpr kLogTextLeftScreenX = 1;

int LogBodyFirstScreenY() {
  return kWorldBannerScreenRows + 1 /* separator below banner */ +
         1 /* log window title row */;
}

struct LogCell {
  int line = 0;
  int col = 0;
};

bool LogCellLess(LogCell const &a, LogCell const &b) {
  return a.line < b.line || (a.line == b.line && a.col < b.col);
}

std::optional<LogCell> HitLogBodyCell(int mx, int my,
                                      std::vector<std::string> const &lines,
                                      int display_start, int visible) {
  int const n = static_cast<int>(lines.size());
  if (visible <= 0 || n <= 0) {
    return std::nullopt;
  }
  int const y0 = LogBodyFirstScreenY();
  if (my < y0 || my >= y0 + visible) {
    return std::nullopt;
  }
  int const rel = my - y0;
  int const line_index = display_start + rel;
  if (line_index < 0 || line_index >= n) {
    return std::nullopt;
  }
  std::string const row =
      StripTerminalAnsi(lines[static_cast<std::size_t>(line_index)]);
  int col = mx - kLogTextLeftScreenX;
  col = std::clamp(col, 0, static_cast<int>(row.size()));
  return LogCell{line_index, col};
}

LogCell PickExtentLogCell(int mx, int my, std::vector<std::string> const &lines,
                          int display_start, int visible, int n) {
  if (auto h = HitLogBodyCell(mx, my, lines, display_start, visible)) {
    return *h;
  }
  int const y0 = LogBodyFirstScreenY();
  if (visible <= 0 || n <= 0) {
    return LogCell{0, 0};
  }
  if (my < y0) {
    return LogCell{display_start, 0};
  }
  if (my >= y0 + visible) {
    int const li = std::clamp(display_start + visible - 1, 0, n - 1);
    std::string const row =
        StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
    return LogCell{li, static_cast<int>(row.size())};
  }
  int const rel = my - y0;
  int li = display_start + rel;
  li = std::clamp(li, 0, n - 1);
  std::string const row =
      StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
  int col = std::clamp(mx - kLogTextLeftScreenX, 0, static_cast<int>(row.size()));
  return LogCell{li, col};
}

std::string BuildLogSelectionText(std::vector<std::string> const &lines, LogCell a,
                                  LogCell b) {
  if (a.line == b.line && a.col == b.col) {
    return {};
  }
  if (!LogCellLess(a, b)) {
    std::swap(a, b);
  }
  int const ls = a.line;
  int const cs = a.col;
  int const le = b.line;
  int const ce = b.col;
  std::string out;
  out.reserve(256);
  for (int L = ls; L <= le; ++L) {
    std::string const row =
        StripTerminalAnsi(lines[static_cast<std::size_t>(L)]);
    int const rn = static_cast<int>(row.size());
    if (ls == le) {
      int const c0 = std::clamp(cs, 0, rn);
      int const c1 = std::clamp(ce, 0, rn);
      if (c0 < c1) {
        out.append(row.substr(static_cast<std::size_t>(c0),
                              static_cast<std::size_t>(c1 - c0)));
      }
    } else if (L == ls) {
      int const c0 = std::clamp(cs, 0, rn);
      out.append(row.substr(static_cast<std::size_t>(c0)));
      out.push_back('\n');
    } else if (L == le) {
      int const c1 = std::clamp(ce, 0, rn);
      out.append(row.substr(0, static_cast<std::size_t>(c1)));
    } else {
      out.append(row);
      out.push_back('\n');
    }
  }
  return out;
}

void SelectionUnderlineSpanOnRow(int row_index, LogCell lo, LogCell hi,
                                 std::string const &row_plain, int *u0, int *u1) {
  int const rn = static_cast<int>(row_plain.size());
  *u0 = *u1 = 0;
  if (!LogCellLess(lo, hi)) {
    std::swap(lo, hi);
  }
  if (row_index < lo.line || row_index > hi.line) {
    return;
  }
  if (lo.line == hi.line) {
    int const a = std::clamp(lo.col, 0, rn);
    int const b = std::clamp(hi.col, 0, rn);
    if (a < b) {
      *u0 = a;
      *u1 = b;
    }
    return;
  }
  if (row_index == lo.line) {
    *u0 = std::clamp(lo.col, 0, rn);
    *u1 = rn;
  } else if (row_index == hi.line) {
    *u0 = 0;
    *u1 = std::clamp(hi.col, 0, rn);
  } else {
    *u0 = 0;
    *u1 = rn;
  }
}

class FtxuiLogSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit FtxuiLogSink(std::size_t maxLines) : max_lines_(maxLines) {
    set_pattern("[%H:%M:%S] [%l]  %v");
  }

  /// Thread-safe; call from UI tick only (never from inside spdlog).
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

  std::size_t LineCount() {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return lines_.size();
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

/// While the fullscreen TUI runs, ignore SIGINT so Ctrl+C does not tear down
/// the alternate screen and dump raw scrollback. Exit via `quit` / `.quit`.
struct IgnoreSigIntForTui {
#ifndef _WIN32
  void (*previous_)(int) = SIG_ERR;
  IgnoreSigIntForTui() { previous_ = std::signal(SIGINT, SIG_IGN); }
  ~IgnoreSigIntForTui() {
    if (previous_ != SIG_ERR) {
      std::signal(SIGINT, previous_);
    }
  }
#else
  IgnoreSigIntForTui() = default;
  ~IgnoreSigIntForTui() = default;
#endif
};

/// Same FIRELANDS block + caption as `PrintBanner(BannerType::World)` (Banner.h),
/// rendered with ember / cool-accent colors to match the TUI chrome.
Element WorldTuiBanner() {
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
      text("WORLD SERVER") | bold | color(Color::RGB(100, 205, 248)),
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

/// After fullscreen TUI teardown, INFO lines on stdout look like the server is
/// still “in the foreground”; keep file (and other) sinks, mute terminal only.
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

int ComputeWorldLogViewportHeight() {
  Dimensions const term = Terminal::Size();
  int const term_h = (term.dimy > 2) ? term.dimy : 25;
  int constexpr kBannerBudget = 12;
  int constexpr kBottomChrome = 6;
  return std::max(6, term_h - kBannerBudget - 1 - kBottomChrome);
}

void RunWorldFtxuiConsoleImpl(AsyncNetworkServer &worldServer,
                              WorldInteractiveConsole &interactiveConsole) {
  IgnoreSigIntForTui const ignoreSigIntDuringTui;
  auto screen = ScreenInteractive::Fullscreen();
  std::shared_ptr<FtxuiLogSink> log_sink;
  log_sink = std::make_shared<FtxuiLogSink>(std::size_t(12000));

  ReplaceStdoutColorSinkWith(log_sink);

  // Log pane scroll: -1 follows the live tail; >=0 is the first visible line
  // index within the copied buffer (PgUp/PgDn and mouse wheel; not Arrow keys,
  // those stay for command history on the input).
  int log_view_first = -1;
  bool log_select_drag = false;
  LogCell log_select_anchor{};
  LogCell log_select_extent{};

  std::string command;
  // Default FTXUI input uses `inverted` when focused; combined with our outer
  // bgcolor/color decorators that yields unreadable text/cursor. Use explicit
  // dark-field colors instead (similar idea to InputOption::Spacious()).
  Color const kInputBg = Color::RGB(40, 36, 34);
  Color const kInputBgHover = Color::RGB(46, 42, 40);
  Color const kInputBgFocus = Color::RGB(54, 50, 46);
  Color const kInputFg = Color::RGB(248, 242, 232);
  InputOption input_opts = InputOption::Default();
  input_opts.multiline() = false;
  input_opts.transform = [=](InputState state) {
    Element e = std::move(state.element);
    Color bg = kInputBg;
    if (state.focused) {
      bg = kInputBgFocus;
    } else if (state.hovered) {
      bg = kInputBgHover;
    }
    e |= bgcolor(bg);
    e |= color(kInputFg);
    if (state.is_placeholder) {
      e |= dim;
    }
    return e;
  };
  auto input = Input(&command, " .help   .exit / quit ", input_opts);
  input |= CatchEvent([&](Event e) {
    if (e != Event::Return) {
      return false;
    }
    std::string c = command;
    command.clear();
    while (!c.empty() && (c.back() == ' ' || c.back() == '\t')) {
      c.pop_back();
    }
    size_t i = 0;
    while (i < c.size() && (c[i] == ' ' || c[i] == '\t')) {
      ++i;
    }
    c = c.substr(i);
    if (!c.empty()) {
      interactiveConsole.SubmitLine(std::move(c));
    }
    return true;
  });

  auto container = Container::Vertical({input});

  auto apply_log_scroll_delta = [&](int delta) -> bool {
    int const log_h = ComputeWorldLogViewportHeight();
    int const n = static_cast<int>(log_sink->LineCount());
    int const max_s = std::max(0, n - log_h);
    if (n == 0) {
      return false;
    }
    int const cur =
        (log_view_first < 0) ? max_s : std::clamp(log_view_first, 0, max_s);
    if (log_view_first < 0 && delta > 0) {
      return false;
    }
    int const next = std::clamp(cur + delta, 0, max_s);
    int new_first = next;
    if (next == max_s && delta >= 0) {
      new_first = -1;
    }
    if (new_first == log_view_first) {
      return false;
    }
    log_select_drag = false;
    log_view_first = new_first;
    screen.RequestAnimationFrame();
    return true;
  };

  container |= CatchEvent([&](Event e) {
    int const log_h = ComputeWorldLogViewportHeight();
    constexpr std::size_t kBufferLines = 4000;
    std::vector<std::string> lines = log_sink->CopyRecentLines(kBufferLines);
    int const n = static_cast<int>(lines.size());
    int const max_start = std::max(0, n - log_h);
    int const display_start =
        (log_view_first < 0)
            ? max_start
            : std::clamp(log_view_first, 0, max_start);
    int const visible = std::min(log_h, n - display_start);

    if (e.is_mouse()) {
      Mouse const &m = e.mouse();
      if (m.button == Mouse::WheelUp) {
        log_select_drag = false;
        return apply_log_scroll_delta(-3);
      }
      if (m.button == Mouse::WheelDown) {
        log_select_drag = false;
        return apply_log_scroll_delta(3);
      }
      // Many terminals (incl. macOS trackpad) send Left+Pressed on every cell
      // while dragging; treat that as extending the selection, not a new press.
      if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
        if (log_select_drag) {
          log_select_extent =
              PickExtentLogCell(m.x, m.y, lines, display_start, visible, n);
          screen.RequestAnimationFrame();
          return true;
        }
        if (auto hit = HitLogBodyCell(m.x, m.y, lines, display_start, visible)) {
          log_select_drag = true;
          log_select_anchor = *hit;
          log_select_extent = *hit;
          screen.RequestAnimationFrame();
          return true;
        }
      }
      if (log_select_drag && m.motion == Mouse::Released) {
        log_select_extent =
            PickExtentLogCell(m.x, m.y, lines, display_start, visible, n);
        std::string const clip =
            BuildLogSelectionText(lines, log_select_anchor, log_select_extent);
        log_select_drag = false;
        screen.RequestAnimationFrame();
        if (!clip.empty()) {
          SetSystemClipboardUtf8(clip);
        }
        return true;
      }
      if (log_select_drag && m.motion == Mouse::Pressed &&
          m.button != Mouse::WheelUp && m.button != Mouse::WheelDown &&
          m.button != Mouse::Left) {
        log_select_extent =
            PickExtentLogCell(m.x, m.y, lines, display_start, visible, n);
        screen.RequestAnimationFrame();
        return true;
      }
    }
    if (e == Event::PageUp) {
      int const log_h = ComputeWorldLogViewportHeight();
      return apply_log_scroll_delta(-std::max(1, log_h - 1));
    }
    if (e == Event::PageDown) {
      int const log_h = ComputeWorldLogViewportHeight();
      return apply_log_scroll_delta(std::max(1, log_h - 1));
    }
    if (e.is_character() && e.character().size() == 1 &&
        static_cast<unsigned char>(e.character()[0]) == 3) {
      return true; // Ctrl+C: do not exit fullscreen / restore cooked tty
    }
    return false;
  });

  const Color kAccent = Color::RGB(255, 118, 60);
  const Color kLogBg = Color::RGB(48, 44, 42);
  const Color kLogFg = Color::RGB(235, 232, 225);
  const Color kLogSelectBg = Color::RGB(95, 78, 52);
  const Color kLogSelectFg = Color::RGB(255, 252, 245);
  const Color kShellBg = Color::RGB(28, 26, 24);

  auto root = Renderer(container, [&] {
    int const log_h = ComputeWorldLogViewportHeight();

    constexpr std::size_t kBufferLines = 4000;
    std::vector<std::string> lines = log_sink->CopyRecentLines(kBufferLines);
    int const n = static_cast<int>(lines.size());
    int const max_start = std::max(0, n - log_h);
    int const display_start =
        (log_view_first < 0)
            ? max_start
            : std::clamp(log_view_first, 0, max_start);

    Elements rows;
    int const visible = std::min(log_h, n - display_start);
    rows.reserve(static_cast<std::size_t>(std::max(0, visible)));
    for (int i = 0; i < visible; ++i) {
      int const li = display_start + i;
      std::string const row =
          StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
      Element line_el;
      if (log_select_drag) {
        int u0 = 0;
        int u1 = 0;
        SelectionUnderlineSpanOnRow(li, log_select_anchor, log_select_extent,
                                    row, &u0, &u1);
        if (u0 < u1) {
          line_el =
              hbox({
                  text(row.substr(0, static_cast<std::size_t>(u0))) | color(kLogFg),
                  text(row.substr(static_cast<std::size_t>(u0),
                                  static_cast<std::size_t>(u1 - u0))) |
                      bgcolor(kLogSelectBg) | color(kLogSelectFg) | underlined,
                  text(row.substr(static_cast<std::size_t>(u1))) | color(kLogFg),
              });
        } else {
          line_el = text(row) | color(kLogFg);
        }
      } else {
        line_el = text(row) | color(kLogFg);
      }
      rows.push_back(std::move(line_el));
    }
    if (rows.empty()) {
      rows.push_back(text("(waiting for log output...)") | color(Color::GrayLight));
    }

    Element const log_body = vbox(std::move(rows)) | bgcolor(kLogBg);
    Element const log_title = hbox({
        text(" ") | bgcolor(kAccent),
        text(" log ") | bold | color(Color::RGB(210, 200, 190)),
        text(" · PgUp/PgDn · rueda · arrastrar = copiar") |
            dim | color(Color::RGB(160, 150, 140)),
        filler() | bgcolor(Color::RGB(40, 36, 34)),
    });
    Element const log_area =
        window(log_title, log_body) | size(HEIGHT, EQUAL, log_h);

    Element const input_rail =
        hbox({
            text(" ") | bgcolor(kAccent),
            input->Render() | flex |
                borderStyled(ROUNDED, Color::RGB(100, 90, 82)),
        });

    Element const banner = WorldTuiBanner();

    return vbox({
               banner | notflex,
               separator() | color(Color::RGB(110, 100, 92)),
               log_area,
               filler() | flex,
               separator() | color(Color::RGB(110, 100, 92)),
               input_rail,
           }) |
           bgcolor(kShellBg);
  });

  std::atomic<bool> run_ticks{true};
  std::thread ticker([&] {
    while (run_ticks.load()) {
      worldServer.Update();
      interactiveConsole.ProcessPending();
      if (log_sink->ConsumeRenderDirty()) {
        screen.Post(Event::Custom);
        screen.RequestAnimationFrame();
      }
      if (interactiveConsole.ShutdownRequested()) {
        screen.ExitLoopClosure()();
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
  if (interactiveConsole.ShutdownRequested()) {
    MuteTerminalLogSinks();
  }
}

} // namespace (anonymous)

void RunWorldFtxuiConsole(AsyncNetworkServer &worldServer,
                          WorldInteractiveConsole &interactiveConsole) {
  RunWorldFtxuiConsoleImpl(worldServer, interactiveConsole);
}

} // namespace Firelands
