#pragma once

#include <cstdint>
#include <vector>

namespace Firelands {

struct PersistedActionButton {
  std::uint8_t button = 0;
  std::uint32_t action = 0;
  std::uint8_t type = 0;
};

struct CharacterActionButtonState {
  std::vector<PersistedActionButton> buttons;
};

} // namespace Firelands
