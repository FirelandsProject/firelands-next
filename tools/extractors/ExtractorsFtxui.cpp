#include "ExtractorsFtxui.h"

#include "ExtractorTasks.h"

#include <shared/system/SystemClipboard.h>
#include <shared/tui/FtxuiTextClipboard.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <optional>
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
Color const kLogSelectBg = Color::RGB(95, 78, 52);
Color const kLogSelectFg = Color::RGB(255, 252, 245);
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

// Reserved vertical space for `ComputeLogViewportHeight` (banner + separator + form).
int constexpr kExtractorsBannerAndSepScreenRows = 12;
int constexpr kExtractorsFormPanelScreenRows = 14;

struct ExtLogCell {
  int line = 0;
  int col = 0;
};

bool ExtLogCellLess(ExtLogCell const &a, ExtLogCell const &b) {
  return a.line < b.line || (a.line == b.line && a.col < b.col);
}

bool ExtractorsLogBodyBoxUsable(Box const &b) {
  return b.x_max >= b.x_min && b.y_max >= b.y_min;
}

std::optional<ExtLogCell> HitExtractorsLogBodyCell(int mx, int my, Box const &body,
                                                   std::vector<std::string> const &lines,
                                                   int display_start, int visible) {
  int const n = static_cast<int>(lines.size());
  if (visible <= 0 || n <= 0 || !ExtractorsLogBodyBoxUsable(body) || !body.Contain(mx, my)) {
    return std::nullopt;
  }
  int const rel_y = std::clamp(my - body.y_min, 0, visible - 1);
  int const line_index = display_start + rel_y;
  if (line_index < 0 || line_index >= n) {
    return std::nullopt;
  }
  std::string const row =
      StripTerminalAnsi(lines[static_cast<std::size_t>(line_index)]);
  int const col = std::clamp(mx - body.x_min, 0, static_cast<int>(row.size()));
  return ExtLogCell{line_index, col};
}

ExtLogCell PickExtractorsExtentLogCell(int mx, int my, Box const &body,
                                       std::vector<std::string> const &lines, int display_start,
                                       int visible, int n) {
  if (auto h = HitExtractorsLogBodyCell(mx, my, body, lines, display_start, visible)) {
    return *h;
  }
  if (visible <= 0 || n <= 0 || !ExtractorsLogBodyBoxUsable(body)) {
    return ExtLogCell{0, 0};
  }
  if (my < body.y_min) {
    return ExtLogCell{display_start, 0};
  }
  if (my > body.y_max) {
    int const li = std::clamp(display_start + visible - 1, 0, n - 1);
    std::string const row = StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
    return ExtLogCell{li, static_cast<int>(row.size())};
  }
  int const rel_y = std::clamp(my - body.y_min, 0, visible - 1);
  int const li = std::clamp(display_start + rel_y, 0, n - 1);
  std::string const row = StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
  int const col = std::clamp(mx - body.x_min, 0, static_cast<int>(row.size()));
  return ExtLogCell{li, col};
}

std::string BuildExtractorsLogSelectionText(std::vector<std::string> const &lines, ExtLogCell a,
                                             ExtLogCell b) {
  if (a.line == b.line && a.col == b.col) {
    return {};
  }
  if (!ExtLogCellLess(a, b)) {
    std::swap(a, b);
  }
  int const ls = a.line;
  int const cs = a.col;
  int const le = b.line;
  int const ce = b.col;
  std::string out;
  out.reserve(256);
  for (int L = ls; L <= le; ++L) {
    std::string const row = StripTerminalAnsi(lines[static_cast<std::size_t>(L)]);
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

void ExtractorsSelectionUnderlineSpanOnRow(int row_index, ExtLogCell lo, ExtLogCell hi,
                                          std::string const &row_plain, int *u0, int *u1) {
  int const rn = static_cast<int>(row_plain.size());
  *u0 = *u1 = 0;
  if (!ExtLogCellLess(lo, hi)) {
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
  int constexpr kFooter = 3;
  return std::max(4, term_h - kExtractorsBannerAndSepScreenRows -
                         kExtractorsFormPanelScreenRows - kFooter);
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
  int data_path_cursor = 0;
  int out_path_cursor = 0;

  auto radiobox = Radiobox(&operation_labels, &operation);

  InputOption path_input_opts = InputOption::Default();
  path_input_opts.multiline() = false;
  path_input_opts.cursor_position = &data_path_cursor;
  Component data_field = Firelands::AttachInputClipboard(
      Input(&data_path_input, "WoW client Data folder (contains .MPQ)", path_input_opts),
      data_path_input, data_path_cursor, {.multiline = false});
  path_input_opts.cursor_position = &out_path_cursor;
  Component out_field = Firelands::AttachInputClipboard(
      Input(&out_path_input, "Output folder (created if missing)", path_input_opts),
      out_path_input, out_path_cursor, {.multiline = false});

  int log_view_first = -1;
  bool log_select_drag = false;
  ExtLogCell log_select_anchor{};
  ExtLogCell log_select_extent{};
  Box extractors_log_body_screen;

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
    log_select_drag = false;
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
    log_select_drag = false;
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
      Element line_el;
      if (log_select_drag) {
        int u0 = 0;
        int u1 = 0;
        ExtractorsSelectionUnderlineSpanOnRow(li, log_select_anchor, log_select_extent, row,
                                              &u0, &u1);
        if (u0 < u1) {
          line_el = hbox({
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
      rows.push_back(text("(console output appears here)") | color(Color::GrayLight));
    }

    Element log_body =
        vbox(std::move(rows)) | bgcolor(kLogBg) | reflect(extractors_log_body_screen);
    Element const log_title = hbox({
        text(" ") | bgcolor(kAccent),
        text(" extractor output ") | bold | color(Color::RGB(210, 200, 190)),
        text(" · PgUp/PgDn · wheel · drag in log = copy ") | dim |
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
    int const log_h = ComputeLogViewportHeight();
    constexpr std::size_t kBufferLines = 8000;
    std::vector<std::string> lines = log_lines.CopyRecent(kBufferLines);
    int const n = static_cast<int>(lines.size());
    int const max_start = std::max(0, n - log_h);
    int const display_start =
        (log_view_first < 0) ? max_start : std::clamp(log_view_first, 0, max_start);
    int const visible = std::min(log_h, n - display_start);

    if (e.is_mouse()) {
      Mouse const &m = e.mouse();
      if (m.button == Mouse::WheelUp || m.button == Mouse::WheelDown) {
        int const delta = (m.button == Mouse::WheelUp) ? -3 : 3;
        return scroll_log(delta);
      }
      if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
        if (log_select_drag) {
          log_select_extent =
              PickExtractorsExtentLogCell(m.x, m.y, extractors_log_body_screen, lines,
                                          display_start, visible, n);
          screen.RequestAnimationFrame();
          return true;
        }
        if (auto hit = HitExtractorsLogBodyCell(m.x, m.y, extractors_log_body_screen, lines,
                                                display_start, visible)) {
          log_select_drag = true;
          log_select_anchor = *hit;
          log_select_extent = *hit;
          screen.RequestAnimationFrame();
          return true;
        }
      }
      if (log_select_drag && m.motion == Mouse::Released) {
        log_select_extent =
            PickExtractorsExtentLogCell(m.x, m.y, extractors_log_body_screen, lines,
                                        display_start, visible, n);
        std::string const clip =
            BuildExtractorsLogSelectionText(lines, log_select_anchor, log_select_extent);
        log_select_drag = false;
        screen.RequestAnimationFrame();
        if (!clip.empty()) {
          Firelands::SetSystemClipboardUtf8(clip);
        }
        return true;
      }
      if (log_select_drag && m.motion == Mouse::Pressed &&
          m.button != Mouse::WheelUp && m.button != Mouse::WheelDown &&
          m.button != Mouse::Left) {
        log_select_extent =
            PickExtractorsExtentLogCell(m.x, m.y, extractors_log_body_screen, lines,
                                        display_start, visible, n);
        screen.RequestAnimationFrame();
        return true;
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
