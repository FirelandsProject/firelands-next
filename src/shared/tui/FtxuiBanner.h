#pragma once

#include <ftxui/dom/elements.hpp>
#include <string_view>

namespace Firelands {

/// FIRELANDS block logo + caption; `server_label` is shown in the cyan accent
/// (e.g. "AUTH SERVER", "WORLD SERVER").
ftxui::Element FirelandsTuiBanner(std::string_view server_label);

} // namespace Firelands
