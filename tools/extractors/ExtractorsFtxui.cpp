#include "ExtractorsFtxui.h"

#include "ExtractorTasks.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#else
#include <cstdio>
#include <io.h>
#endif

namespace firelands::extract {
namespace {

namespace fs = std::filesystem;
using namespace ftxui;

Color const kShellBg = Color::RGB(26, 24, 22);
Color const kLogBg = Color::RGB(28, 26, 24);
Color const kLogFg = Color::RGB(210, 204, 196);
Color const kAccent = Color::RGB(180, 72, 52);

int constexpr kBannerScreenRows = 11;

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

/// Same FIRELANDS block + caption style as auth/world TUIs; label tuned for extractors.
Element ExtractorsTuiBanner() {
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
      text("CLIENT DATA EXTRACTORS") | bold | color(Color::RGB(100, 205, 248)),
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

class LineRingBuffer final {
public:
  explicit LineRingBuffer(std::size_t maxLines) : max_lines_(maxLines) {}

  void Push(std::string line) {
    std::lock_guard<std::mutex> lock(mu_);
    lines_.push_back(std::move(line));
    while (lines_.size() > max_lines_) {
      lines_.pop_front();
    }
    dirty_.store(true, std::memory_order_release);
  }

  bool ConsumeRenderDirty() {
    return dirty_.exchange(false, std::memory_order_acq_rel);
  }

  std::vector<std::string> CopyRecent(std::size_t maxRender) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (lines_.size() <= maxRender) {
      return std::vector<std::string>(lines_.begin(), lines_.end());
    }
    return std::vector<std::string>(lines_.end() - maxRender, lines_.end());
  }

  std::size_t LineCount() const {
    std::lock_guard<std::mutex> lock(mu_);
    return lines_.size();
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    lines_.clear();
    dirty_.store(true, std::memory_order_release);
  }

private:
  std::size_t max_lines_;
  mutable std::mutex mu_;
  std::deque<std::string> lines_;
  std::atomic<bool> dirty_{false};
};

/// Forwards ostream writes into complete lines (newline or flush).
class LineStreamBuf final : public std::streambuf {
public:
  LineStreamBuf(LineRingBuffer *sink, std::function<void()> on_activity)
      : sink_(sink), on_activity_(std::move(on_activity)) {}

protected:
  std::streamsize xsputn(char const *s, std::streamsize n) override {
    for (std::streamsize i = 0; i < n; ++i) {
      if (sputc(s[i]) == traits_type::eof()) {
        return i;
      }
    }
    return n;
  }

  int_type overflow(int_type ch) override {
    if (traits_type::eq_int_type(ch, traits_type::eof())) {
      return traits_type::not_eof(0);
    }
    char const c = traits_type::to_char_type(ch);
    if (c == '\r') {
      return ch;
    }
    if (c == '\n') {
      FlushPending();
      return ch;
    }
    pending_.push_back(c);
    return ch;
  }

  int sync() override {
    FlushPending();
    return 0;
  }

private:
  void FlushPending() {
    if (sink_ == nullptr || pending_.empty()) {
      return;
    }
    sink_->Push(std::string(pending_.begin(), pending_.end()));
    pending_.clear();
    if (on_activity_) {
      on_activity_();
    }
  }

  LineRingBuffer *sink_;
  std::function<void()> on_activity_;
  std::vector<char> pending_;
};

int ComputeLogViewportHeight() {
  Dimensions const term = Terminal::Size();
  int const term_h = (term.dimy > 2) ? term.dimy : 25;
  int constexpr kBannerBudget = 12;
  int constexpr kFormChrome = 14;
  int constexpr kFooter = 3;
  return std::max(4, term_h - kBannerBudget - kFormChrome - kFooter);
}

bool StdoutIsInteractiveTerminal() {
#ifndef _WIN32
  return ::isatty(STDOUT_FILENO) != 0;
#else
  return ::_isatty(::_fileno(stdout)) != 0;
#endif
}

int RunExtractorsFtxuiImpl() {
  if (!StdoutIsInteractiveTerminal()) {
    std::cerr << "firelands-extractors TUI requires an interactive terminal "
                 "(TTY).\n"
                 "For scripts use:\n"
                 "  firelands-dbc-extractor         --data <WoW/Data> --out <dir>\n"
                 "  firelands-map-extractor        --data <WoW/Data> --out <dir>\n"
                 "  firelands-map-extractor-vmap   -d <WoW> -o <dir>   (install root; adds Data/)\n"
                 "  firelands-vmap4-extractor      -d <WoW-install> -o <collision-root>\n"
                 "  firelands-vmap4-assembler      [Buildings-dir] [vmaps-dir]\n";
    return 2;
  }

  IgnoreSigIntForTui const ignoreSigIntDuringTui;

  auto screen = ScreenInteractive::Fullscreen();
  Closure const requestExit = screen.ExitLoopClosure();

  LineRingBuffer log_lines(16000);
  std::atomic<bool> job_running{false};
  std::thread worker;

  auto notify_log = [&] { screen.Post(Event::Custom); };

  auto append_system_line = [&](std::string const &msg) {
    log_lines.Push(msg);
    notify_log();
  };

  int operation = 0;
  std::vector<std::string> operation_labels = {
      "Extract DBFilesClient (*.dbc, *.db2)",
      "Extract map assets (World/maps …)",
      "List MPQ patch order only (no extraction)",
      "Server maps for collision (.map / .tilelist / Cameras/)",
      "VMAP4 extract (Buildings/ — firelands-vmap4-extractor)",
      "VMAP4 assemble (Buildings/ → vmaps/ — firelands-vmap4-assembler)",
  };

  std::string data_path_input;
  std::string out_path_input;

  auto radiobox = Radiobox(&operation_labels, &operation);

  Component data_field = Input(&data_path_input, "WoW client Data folder (contains .MPQ)");
  Component out_field =
      Input(&out_path_input, "Output folder (created if missing)");

  auto run_clicked = [&]() {
    if (job_running.load(std::memory_order_acquire)) {
      return;
    }

    fs::path data_dir(data_path_input);
    fs::path out_dir(out_path_input);

    if (data_dir.empty() || !fs::exists(data_dir) || !fs::is_directory(data_dir)) {
      append_system_line("[error] WoW Data path is missing or not a directory.");
      return;
    }

    int const op_sel = operation;
    bool const list_only = (op_sel == 2);
    if (!list_only && out_dir.empty()) {
      append_system_line("[error] Output folder is required for extraction.");
      return;
    }

    job_running.store(true, std::memory_order_release);
    append_system_line("[info] Starting…");

    worker = std::thread([&, op_sel, list_only, data_dir, out_dir] {
      LineStreamBuf out_buf(&log_lines, notify_log);
      LineStreamBuf err_buf(&log_lines, notify_log);
      std::ostream out_stream(&out_buf);
      std::ostream err_stream(&err_buf);

      int rc = 1;
      if (list_only) {
        rc = RunListMpqsTask(data_dir, out_stream, err_stream);
      } else if (op_sel == 0) {
        rc = RunDbcExtractTask(data_dir, out_dir, out_stream, err_stream);
      } else if (op_sel == 1) {
        rc = RunMapExtractTask(data_dir, out_dir, out_stream, err_stream);
      } else if (op_sel == 3) {
        rc = RunServerMapVmapExtractTask(data_dir, out_dir, out_stream, err_stream);
      } else if (op_sel == 4) {
        rc = RunVmap4ExtractorSubprocess(data_dir, out_dir, out_stream, err_stream);
      } else {
        rc = RunVmap4AssemblerSubprocess(out_dir, out_stream, err_stream);
      }

      out_stream.flush();
      err_stream.flush();

      std::ostringstream summary;
      summary << "[info] Finished"
              << (rc == 0 ? " successfully." : " with errors (non-zero exit).")
              << " Code: " << rc;
      log_lines.Push(summary.str());
      log_lines.Push(std::string());

      screen.Post(Event::Custom);
      screen.Post([&] {
        job_running.store(false, std::memory_order_release);
        if (worker.joinable()) {
          worker.join();
        }
      });
    });
  };

  auto clear_log_clicked = [&]() {
    if (job_running.load(std::memory_order_acquire)) {
      return;
    }
    log_lines.Clear();
    notify_log();
  };

  Component run_btn = Button("Run", run_clicked);
  Component clear_btn = Button("Clear console", clear_log_clicked);

  auto form = Container::Vertical({
      radiobox,
      data_field,
      out_field,
      Container::Horizontal({
          run_btn,
          clear_btn,
      }),
  });

  int log_view_first = -1;

  auto scroll_log = [&](int delta) -> bool {
    int const log_h = ComputeLogViewportHeight();
    int const n = static_cast<int>(log_lines.LineCount());
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
    log_view_first = new_first;
    screen.RequestAnimationFrame();
    return true;
  };

  auto root = Renderer(form, [&]() -> Element {
    bool const busy = job_running.load(std::memory_order_acquire);
    Element const status_note =
        busy ? text(" Running… ") | bgcolor(Color::RGB(80, 55, 40)) | bold
             : text(" Ready ") | dim;

    Element out_render = out_field->Render();
    if (operation == 2) {
      out_render = out_render | dim;
    }

    Element run_el = run_btn->Render();
    Element clear_el = clear_btn->Render();
    if (busy) {
      run_el = run_el | dim;
      clear_el = clear_el | dim;
    }

    Element const form_panel =
        window(text(" Task & paths ") | bold,
               vbox({
                   hbox({text(" Operation: "), status_note, filler()}),
                   radiobox->Render(),
                   separator(),
                   data_field->Render(),
                   separator(),
                   out_render,
                   separator(),
                   hbox({
                       run_el,
                       text("  "),
                       clear_el,
                       filler(),
                   }),
               }));

    int const log_h = ComputeLogViewportHeight();
    constexpr std::size_t kBufferLines = 8000;
    std::vector<std::string> lines = log_lines.CopyRecent(kBufferLines);
    int const n = static_cast<int>(lines.size());
    int const max_start = std::max(0, n - log_h);
    int const display_start =
        (log_view_first < 0) ? max_start : std::clamp(log_view_first, 0, max_start);

    Elements rows;
    int const visible = std::min(log_h, n - display_start);
    rows.reserve(static_cast<std::size_t>(std::max(0, visible)));
    for (int i = 0; i < visible; ++i) {
      int const li = display_start + i;
      std::string const row =
          StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
      rows.push_back(text(row) | color(kLogFg));
    }
    if (rows.empty()) {
      rows.push_back(text("(console output appears here)") | color(Color::GrayLight));
    }

    Element const log_body = vbox(std::move(rows)) | bgcolor(kLogBg);
    Element const log_title = hbox({
        text(" ") | bgcolor(kAccent),
        text(" extractor output ") | bold | color(Color::RGB(210, 200, 190)),
        text(" · PgUp/PgDn · mouse wheel ") | dim |
            color(Color::RGB(160, 150, 140)),
        filler() | bgcolor(Color::RGB(40, 36, 34)),
    });
    Element const log_area =
        window(log_title, log_body) | size(HEIGHT, EQUAL, log_h) | flex;

    Element const footer = hbox({
        text(" ") | bgcolor(kAccent),
        text("  Q  quit when idle  ") | bold | color(Color::RGB(248, 242, 232)),
        text("  ·  Ctrl+C ignored (restore on exit)  ") |
            dim | color(Color::RGB(180, 170, 160)),
        filler() | bgcolor(Color::RGB(36, 34, 32)),
    });

    Element const banner = ExtractorsTuiBanner();

    return vbox({
               banner | notflex,
               separator() | color(Color::RGB(110, 100, 92)),
               form_panel | notflex,
               separator() | color(Color::RGB(110, 100, 92)),
               log_area,
               filler() | flex,
               separator() | color(Color::RGB(110, 100, 92)),
               footer,
           }) |
           bgcolor(kShellBg);
  });

  root = CatchEvent(root, [&](Event e) {
    if (e.is_mouse()) {
      Mouse const &me = e.mouse();
      if (me.button == Mouse::WheelUp || me.button == Mouse::WheelDown) {
        int const delta = (me.button == Mouse::WheelUp) ? -3 : 3;
        return scroll_log(delta);
      }
    }
    if (e == Event::PageUp) {
      return scroll_log(-ComputeLogViewportHeight());
    }
    if (e == Event::PageDown) {
      return scroll_log(ComputeLogViewportHeight());
    }
    if (!e.is_character() || e.character().size() != 1) {
      return false;
    }
    char const c = e.character()[0];
    if (c == 'q' || c == 'Q') {
      if (job_running.load(std::memory_order_acquire)) {
        return true;
      }
      if (worker.joinable()) {
        worker.join();
      }
      requestExit();
      return true;
    }
    return false;
  });

  std::atomic<bool> run_ticks{true};
  std::thread ticker([&] {
    while (run_ticks.load()) {
      if (log_lines.ConsumeRenderDirty()) {
        screen.Post(Event::Custom);
        screen.RequestAnimationFrame();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
  });

  screen.Loop(root);

  run_ticks = false;
  if (ticker.joinable()) {
    ticker.join();
  }

  if (worker.joinable()) {
    worker.join();
  }

  return 0;
}

} // namespace

int RunExtractorsFtxui() {
  return RunExtractorsFtxuiImpl();
}

} // namespace firelands::extract
