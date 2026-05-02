#pragma once

#include <string_view>

namespace Firelands {

/// Writes UTF-8 text to the system clipboard (best-effort, no exceptions).
/// Windows: CF_UNICODETEXT via Win32. macOS: pbcopy. Linux: wl-copy, xclip, or xsel.
void SetSystemClipboardUtf8(std::string_view utf8);

} // namespace Firelands
