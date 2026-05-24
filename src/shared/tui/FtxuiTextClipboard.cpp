#include "FtxuiTextClipboard.h"

#include <shared/system/SystemClipboard.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

namespace Firelands {
namespace {

constexpr unsigned char kCtrlA = 1;
constexpr unsigned char kCtrlC = 3;
constexpr unsigned char kCtrlV = 22;
constexpr unsigned char kCtrlX = 24;

void ClampCursor(std::string const &text, int &cursor) {
  if (cursor < 0) {
    cursor = 0;
  }
  if (cursor > static_cast<int>(text.size())) {
    cursor = static_cast<int>(text.size());
  }
}

} // namespace

void InsertTextAtCursor(std::string &text, int &cursor, std::string_view chunk,
                          TextClipboardOptions options) {
  if (chunk.empty()) {
    return;
  }
  ClampCursor(text, cursor);

  std::string sanitized;
  sanitized.reserve(chunk.size());
  for (char raw : chunk) {
    unsigned char const c = static_cast<unsigned char>(raw);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (options.multiline) {
        sanitized.push_back('\n');
      } else if (!sanitized.empty() && sanitized.back() != ' ') {
        sanitized.push_back(' ');
      }
      continue;
    }
    if (c == '\t' && options.multiline) {
      sanitized.push_back('\t');
      continue;
    }
    if (c >= 32 && c != 127) {
      sanitized.push_back(static_cast<char>(c));
    }
  }
  if (sanitized.empty()) {
    return;
  }

  text.insert(static_cast<std::size_t>(cursor), sanitized);
  cursor += static_cast<int>(sanitized.size());
}

bool TryHandleTextClipboardEvent(ftxui::Event const &event, std::string &text,
                                 int &cursor, TextClipboardOptions options) {
  if (!event.is_character()) {
    return false;
  }

  std::string const &input = event.character();
  if (input.size() > 1) {
    InsertTextAtCursor(text, cursor, input, options);
    return true;
  }
  if (input.empty()) {
    return false;
  }

  unsigned char const c = static_cast<unsigned char>(input[0]);
  if (c == kCtrlA) {
    cursor = 0;
    return true;
  }
  if (c == kCtrlC) {
    if (!text.empty()) {
      SetSystemClipboardUtf8(text);
    }
    return true;
  }
  if (c == kCtrlX) {
    if (!text.empty()) {
      SetSystemClipboardUtf8(text);
      text.clear();
      cursor = 0;
    }
    return true;
  }
  if (c == kCtrlV) {
    if (auto clip = GetSystemClipboardUtf8()) {
      InsertTextAtCursor(text, cursor, *clip, options);
    }
    return true;
  }
  if (c < 32 && c != '\t') {
    return true;
  }
  return false;
}

ftxui::Component AttachInputClipboard(ftxui::Component input, std::string &content,
                                      int &cursor_position, TextClipboardOptions options) {
  input |= ftxui::CatchEvent([&](ftxui::Event const &event) {
    return TryHandleTextClipboardEvent(event, content, cursor_position, options);
  });
  return input;
}

} // namespace Firelands
