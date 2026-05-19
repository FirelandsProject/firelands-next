#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/sinks/base_sink.h>

namespace Firelands {

/// spdlog sink that buffers formatted lines for an FTXUI log pane.
class FtxuiLogSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit FtxuiLogSink(std::size_t maxLines);

  /// Thread-safe; call from UI tick only (never from inside spdlog).
  bool ConsumeRenderDirty();

  std::vector<std::string> CopyRecentLines(std::size_t maxRender);
  std::size_t LineCount();

protected:
  void sink_it_(const spdlog::details::log_msg &msg) override;
  void flush_() override;

private:
  std::size_t max_lines_;
  std::deque<std::string> lines_;
  std::atomic<bool> dirty_{false};
};

using FtxuiLogSinkPtr = std::shared_ptr<FtxuiLogSink>;

} // namespace Firelands
