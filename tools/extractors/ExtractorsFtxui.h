#pragma once

namespace firelands::extract {

/// Full-screen extractor launcher (FTXUI): Firelands banner, form to pick task /
/// paths, and a scrollable console for captured extractor output.
/// Exit with **Q** when idle (Ctrl+C ignored while fullscreen, same as auth/world).
int RunExtractorsFtxui();

} // namespace firelands::extract
