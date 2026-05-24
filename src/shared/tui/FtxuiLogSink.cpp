#include <shared/tui/FtxuiLogSink.h>

#include <spdlog/details/log_msg.h>

namespace Firelands {

FtxuiLogSink::FtxuiLogSink(std::size_t maxLines) : max_lines_(maxLines) {
  // Pattern is set from Firelands Logger via BindFirelandsLoggerToFtxuiSink().
  set_pattern("%^[%H:%M:%S] [%l]%$  %v");
}

bool FtxuiLogSink::ConsumeRenderDirty() {
  return dirty_.exchange(false, std::memory_order_acq_rel);
}

std::vector<std::string> FtxuiLogSink::CopyRecentLines(std::size_t maxRender) {
  std::lock_guard<std::mutex> lock(this->mutex_);
  if (lines_.size() <= maxRender) {
    return std::vector<std::string>(lines_.begin(), lines_.end());
  }
  return std::vector<std::string>(lines_.end() - maxRender, lines_.end());
}

std::size_t FtxuiLogSink::LineCount() {
  std::lock_guard<std::mutex> lock(this->mutex_);
  return lines_.size();
}

void FtxuiLogSink::sink_it_(const spdlog::details::log_msg &msg) {
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

void FtxuiLogSink::flush_() {}

} // namespace Firelands
