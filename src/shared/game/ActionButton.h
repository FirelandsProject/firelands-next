#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace Firelands {
class WorldPacket;
}

namespace Firelands::ActionButton {

struct SetActionButtonCmsg {
  uint8_t index = 0;
  uint32_t packedAction = 0;
};

/// Cataclysm 4.3.4: `MAX_ACTION_BUTTONS` (144 in 15595).
constexpr size_t kMaxButtons = 144;
/// Dual talent specialization action bar sets (Cataclysm).
constexpr size_t kMaxActionBarSpecs = 2;
constexpr uint32_t kMaxActionValue = 0x00FFFFFFu + 1u;

enum Type : uint8_t {
  Spell = 0x00,
  Click = 0x01,
  Macro = 0x40,
  ClickMacro = 0x41,
  Item = 0x80,
};

constexpr uint32_t Pack(uint32_t action, uint8_t type) {
  return (action & 0x00FFFFFFu) | (static_cast<uint32_t>(type) << 24);
}

constexpr uint32_t ActionFromPacked(uint32_t packed) { return packed & 0x00FFFFFFu; }
constexpr uint8_t TypeFromPacked(uint32_t packed) {
  return static_cast<uint8_t>((packed >> 24) & 0xFFu);
}

constexpr uint32_t PackWire(uint32_t action, uint8_t type) {
  return Pack(action, type);
}

constexpr uint32_t PackFromClientAction(uint32_t clientAction) {
  return clientAction;
}

using PackedActionBar = std::array<uint32_t, kMaxButtons>;

bool IsValidButtonIndex(uint8_t button);
bool IsValidActionValue(uint32_t action);
bool IsKnownType(uint8_t type);

/// Returns false for invalid wire data (out-of-range button/action or unknown type).
bool IsValidSetRequest(uint8_t button, uint32_t packedAction);

/// Parses `CMSG_SET_ACTION_BUTTON` (Cataclysm 4.3.4: uint32 action, uint8 index).
bool TryParseSetActionButtonCmsg(WorldPacket &packet, SetActionButtonCmsg &out);

/// Spell buttons require `knownSpellIds`; other allowed types skip spellbook checks.
bool MayPlaceOnBar(uint8_t type, uint32_t action,
                   std::unordered_set<uint32_t> const &knownSpellIds);

void ClearBar(PackedActionBar &packed);

} // namespace Firelands::ActionButton
