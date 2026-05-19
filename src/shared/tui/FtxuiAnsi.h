#pragma once

#include <string>

namespace Firelands {

/// Remove CSI SGR sequences so FTXUI text() layout matches plain columns.
std::string StripTerminalAnsi(std::string const &in);

} // namespace Firelands
