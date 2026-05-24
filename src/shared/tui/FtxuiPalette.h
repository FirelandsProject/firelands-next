#pragma once

#include <ftxui/screen/color.hpp>

// <windows.h> defines RGB as a macro; undefine it so ftxui::Color::RGB compiles.
#ifdef RGB
#undef RGB
#endif

namespace Firelands {

struct FtxuiServerPalette {
  static ftxui::Color Accent() { return ftxui::Color::RGB(255, 118, 60); }
  static ftxui::Color ShellBg() { return ftxui::Color::RGB(28, 26, 24); }
  static ftxui::Color LogBg() { return ftxui::Color::RGB(48, 44, 42); }
  static ftxui::Color LogFg() { return ftxui::Color::RGB(235, 232, 225); }
  static ftxui::Color LogSelectBg() { return ftxui::Color::RGB(95, 78, 52); }
  static ftxui::Color LogSelectFg() { return ftxui::Color::RGB(255, 252, 245); }
  static ftxui::Color Separator() { return ftxui::Color::RGB(110, 100, 92); }
  static ftxui::Color InputBg() { return ftxui::Color::RGB(40, 36, 34); }
  static ftxui::Color InputBgHover() { return ftxui::Color::RGB(46, 42, 40); }
  static ftxui::Color InputBgFocus() { return ftxui::Color::RGB(54, 50, 46); }
  static ftxui::Color InputFg() { return ftxui::Color::RGB(248, 242, 232); }
};

} // namespace Firelands
