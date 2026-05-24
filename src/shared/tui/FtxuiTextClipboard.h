#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <string>

namespace Firelands {

struct TextClipboardOptions {
  bool multiline = false;
};

/// Inserts sanitized text at the cursor (handles terminal paste and multi-byte input).
void InsertTextAtCursor(std::string &text, int &cursor, std::string_view chunk,
                          TextClipboardOptions options);

/// Handles Ctrl+C/V/X/A and terminal paste for custom text fields.
/// Returns true when the event was consumed.
bool TryHandleTextClipboardEvent(ftxui::Event const &event, std::string &text,
                                 int &cursor, TextClipboardOptions options);

/// Adds clipboard shortcuts to an FTXUI Input (uses the same cursor ref as InputOption).
ftxui::Component AttachInputClipboard(ftxui::Component input, std::string &content,
                                      int &cursor_position,
                                      TextClipboardOptions options = {});

} // namespace Firelands
