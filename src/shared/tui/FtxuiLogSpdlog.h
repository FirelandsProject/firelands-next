#pragma once

#include <memory>

#include <spdlog/sinks/sink.h>

namespace Firelands {

void ReplaceStdoutColorSinkWith(spdlog::sink_ptr replacement);
void RestoreStdoutColorSink(spdlog::sink_ptr ftxui_sink);

/// After fullscreen TUI teardown, mute terminal sinks so post-exit INFO lines
/// do not look like the server is still in the foreground.
void MuteTerminalLogSinks();

} // namespace Firelands
