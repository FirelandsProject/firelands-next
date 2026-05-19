#include <shared/tui/FtxuiLogView.h>

#include <shared/tui/FtxuiAnsi.h>
#include <shared/system/SystemClipboard.h>

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace Firelands {
namespace {

using namespace ftxui;

bool LogCellLess(FtxuiLogCell const &a, FtxuiLogCell const &b) {
  return a.line < b.line || (a.line == b.line && a.col < b.col);
}

std::optional<FtxuiLogCell> HitLogBodyCell(FtxuiLogViewLayout const &layout,
                                           int mx, int my,
                                           std::vector<std::string> const &lines,
                                           int display_start, int visible) {
  int const n = static_cast<int>(lines.size());
  if (visible <= 0 || n <= 0) {
    return std::nullopt;
  }
  int const y0 = layout.LogBodyFirstScreenY();
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
  int col = mx - layout.log_text_left_screen_x;
  col = std::clamp(col, 0, static_cast<int>(row.size()));
  return FtxuiLogCell{line_index, col};
}

FtxuiLogCell PickExtentLogCell(FtxuiLogViewLayout const &layout, int mx, int my,
                               std::vector<std::string> const &lines,
                               int display_start, int visible, int n) {
  if (auto h = HitLogBodyCell(layout, mx, my, lines, display_start, visible)) {
    return *h;
  }
  int const y0 = layout.LogBodyFirstScreenY();
  if (visible <= 0 || n <= 0) {
    return FtxuiLogCell{0, 0};
  }
  if (my < y0) {
    return FtxuiLogCell{display_start, 0};
  }
  if (my >= y0 + visible) {
    int const li = std::clamp(display_start + visible - 1, 0, n - 1);
    std::string const row =
        StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
    return FtxuiLogCell{li, static_cast<int>(row.size())};
  }
  int const rel = my - y0;
  int li = display_start + rel;
  li = std::clamp(li, 0, n - 1);
  std::string const row =
      StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
  int col =
      std::clamp(mx - layout.log_text_left_screen_x, 0, static_cast<int>(row.size()));
  return FtxuiLogCell{li, col};
}

std::string BuildLogSelectionText(std::vector<std::string> const &lines,
                                  FtxuiLogCell a, FtxuiLogCell b) {
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

void SelectionUnderlineSpanOnRow(int row_index, FtxuiLogCell lo, FtxuiLogCell hi,
                                 std::string const &row_plain, int *u0,
                                 int *u1) {
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

constexpr std::size_t kLogRenderBufferLines = 4000;

} // namespace

int FtxuiLogViewLayout::LogBodyFirstScreenY() const {
  return banner_screen_rows + separator_below_banner_rows +
         log_window_title_rows;
}

int ComputeFtxuiLogViewportHeight(int bottom_chrome_rows, int banner_budget) {
  Dimensions const term = Terminal::Size();
  int const term_h = (term.dimy > 2) ? term.dimy : 25;
  return std::max(6, term_h - banner_budget - 1 - bottom_chrome_rows);
}

FtxuiLogView::FtxuiLogView(FtxuiLogViewLayout layout, FtxuiLogSinkPtr sink)
    : layout_(layout), sink_(std::move(sink)) {}

int FtxuiLogView::DisplayStart(int log_viewport_height,
                               std::vector<std::string> const &lines) const {
  int const n = static_cast<int>(lines.size());
  int const max_start = std::max(0, n - log_viewport_height);
  return (view_first_ < 0) ? max_start
                           : std::clamp(view_first_, 0, max_start);
}

bool FtxuiLogView::ApplyScrollDelta(int delta, int log_viewport_height) {
  int const n = static_cast<int>(sink_->LineCount());
  int const max_s = std::max(0, n - log_viewport_height);
  if (n == 0) {
    return false;
  }
  int const cur =
      (view_first_ < 0) ? max_s : std::clamp(view_first_, 0, max_s);
  if (view_first_ < 0 && delta > 0) {
    return false;
  }
  int const next = std::clamp(cur + delta, 0, max_s);
  int new_first = next;
  if (next == max_s && delta >= 0) {
    new_first = -1;
  }
  if (new_first == view_first_) {
    return false;
  }
  select_drag_ = false;
  view_first_ = new_first;
  return true;
}

bool FtxuiLogView::HandleEvent(ftxui::Event &event,
                               int log_viewport_height,
                               std::function<void()> request_animation_frame) {
  std::vector<std::string> lines = sink_->CopyRecentLines(kLogRenderBufferLines);
  int const n = static_cast<int>(lines.size());
  int const display_start = DisplayStart(log_viewport_height, lines);
  int const visible = std::min(log_viewport_height, n - display_start);

  if (event.is_mouse()) {
    Mouse const &m = event.mouse();
    if (m.button == Mouse::WheelUp) {
      select_drag_ = false;
      if (ApplyScrollDelta(-3, log_viewport_height)) {
        request_animation_frame();
        return true;
      }
      return false;
    }
    if (m.button == Mouse::WheelDown) {
      select_drag_ = false;
      if (ApplyScrollDelta(3, log_viewport_height)) {
        request_animation_frame();
        return true;
      }
      return false;
    }
    if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
      if (select_drag_) {
        select_extent_ = PickExtentLogCell(layout_, m.x, m.y, lines,
                                           display_start, visible, n);
        request_animation_frame();
        return true;
      }
      if (auto hit = HitLogBodyCell(layout_, m.x, m.y, lines, display_start,
                                    visible)) {
        select_drag_ = true;
        select_anchor_ = *hit;
        select_extent_ = *hit;
        request_animation_frame();
        return true;
      }
    }
    if (select_drag_ && m.motion == Mouse::Released) {
      select_extent_ = PickExtentLogCell(layout_, m.x, m.y, lines, display_start,
                                         visible, n);
      std::string const clip =
          BuildLogSelectionText(lines, select_anchor_, select_extent_);
      select_drag_ = false;
      request_animation_frame();
      if (!clip.empty()) {
        SetSystemClipboardUtf8(clip);
      }
      return true;
    }
    if (select_drag_ && m.motion == Mouse::Pressed &&
        m.button != Mouse::WheelUp && m.button != Mouse::WheelDown &&
        m.button != Mouse::Left) {
      select_extent_ = PickExtentLogCell(layout_, m.x, m.y, lines, display_start,
                                         visible, n);
      request_animation_frame();
      return true;
    }
  }
  if (event == Event::PageUp) {
    if (ApplyScrollDelta(-std::max(1, log_viewport_height - 1),
                         log_viewport_height)) {
      request_animation_frame();
      return true;
    }
    return false;
  }
  if (event == Event::PageDown) {
    if (ApplyScrollDelta(std::max(1, log_viewport_height - 1),
                         log_viewport_height)) {
      request_animation_frame();
      return true;
    }
    return false;
  }
  return false;
}

ftxui::Elements FtxuiLogView::BuildRows(int log_viewport_height) const {
  using namespace ftxui;

  Color const kLogFg = FtxuiServerPalette::LogFg();
  Color const kLogSelectBg = FtxuiServerPalette::LogSelectBg();
  Color const kLogSelectFg = FtxuiServerPalette::LogSelectFg();

  std::vector<std::string> lines = sink_->CopyRecentLines(kLogRenderBufferLines);
  int const n = static_cast<int>(lines.size());
  int const display_start = DisplayStart(log_viewport_height, lines);

  Elements rows;
  int const visible = std::min(log_viewport_height, n - display_start);
  rows.reserve(static_cast<std::size_t>(std::max(0, visible)));
  for (int i = 0; i < visible; ++i) {
    int const li = display_start + i;
    std::string const row =
        StripTerminalAnsi(lines[static_cast<std::size_t>(li)]);
    Element line_el;
    if (select_drag_) {
      int u0 = 0;
      int u1 = 0;
      SelectionUnderlineSpanOnRow(li, select_anchor_, select_extent_, row, &u0,
                                  &u1);
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
  return rows;
}

ftxui::Element FtxuiLogView::Window(int log_viewport_height) const {
  using namespace ftxui;

  Color const kAccent = FtxuiServerPalette::Accent();
  Color const kLogBg = FtxuiServerPalette::LogBg();

  Element const log_body = vbox(BuildRows(log_viewport_height)) | bgcolor(kLogBg);
  Element const log_title = hbox({
      text(" ") | bgcolor(kAccent),
      text(" log ") | bold | color(Color::RGB(210, 200, 190)),
      text(" · PgUp/PgDn · scroll · drag = copy") |
          dim | color(Color::RGB(160, 150, 140)),
      filler() | bgcolor(Color::RGB(40, 36, 34)),
  });
  return window(log_title, log_body) | size(HEIGHT, EQUAL, log_viewport_height);
}

} // namespace Firelands
