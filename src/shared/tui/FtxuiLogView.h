#pragma once

#include <shared/tui/FtxuiLogSink.h>
#include <shared/tui/FtxuiPalette.h>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace Firelands {

/// Screen layout for hit-testing and scrolling the log body (banner + chrome).
struct FtxuiLogViewLayout {
  /// Rows occupied by `FirelandsTuiBanner()` including ROUNDED border (+2).
  int banner_screen_rows = 11;
  int log_text_left_screen_x = 1;
  int separator_below_banner_rows = 1;
  int log_window_title_rows = 1;

  int LogBodyFirstScreenY() const;
};

/// Viewport height for the log body given terminal size and bottom chrome.
int ComputeFtxuiLogViewportHeight(int bottom_chrome_rows,
                                  int banner_budget = 12);

struct FtxuiLogCell {
  int line = 0;
  int col = 0;
};

/// Scrollable, selectable log pane backed by `FtxuiLogSink`.
class FtxuiLogView {
public:
  FtxuiLogView(FtxuiLogViewLayout layout, FtxuiLogSinkPtr sink);

  FtxuiLogViewLayout const &layout() const { return layout_; }
  FtxuiLogSinkPtr const &sink() const { return sink_; }

  int view_first() const { return view_first_; }

  int DisplayStart(int log_viewport_height,
                   std::vector<std::string> const &lines) const;

  bool HandleEvent(ftxui::Event &event, int log_viewport_height,
                   std::function<void()> request_animation_frame);

  ftxui::Elements BuildRows(int log_viewport_height) const;

  ftxui::Element Window(int log_viewport_height) const;

private:
  bool ApplyScrollDelta(int delta, int log_viewport_height);

  FtxuiLogViewLayout layout_;
  FtxuiLogSinkPtr sink_;
  int view_first_ = -1;
  bool select_drag_ = false;
  FtxuiLogCell select_anchor_{};
  FtxuiLogCell select_extent_{};
};

} // namespace Firelands
