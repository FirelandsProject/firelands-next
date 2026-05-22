#include <shared/game/ActionButton.h>
#include <shared/network/WorldPacket.h>

namespace Firelands::ActionButton {

namespace {

bool TryAssignParsed(SetActionButtonCmsg &out, uint8_t index, uint32_t packedAction) {
  if (!IsValidSetRequest(index, packedAction))
    return false;
  out.index = index;
  out.packedAction = packedAction;
  return true;
}

} // namespace

bool IsValidButtonIndex(uint8_t button) { return button < kMaxButtons; }

bool IsValidActionValue(uint32_t action) { return action < kMaxActionValue; }

bool IsKnownType(uint8_t type) {
  switch (type) {
  case Spell:
  case Click:
  case Macro:
  case ClickMacro:
  case Item:
    return true;
  default:
    return false;
  }
}

bool IsValidSetRequest(uint8_t button, uint32_t packedAction) {
  if (!IsValidButtonIndex(button))
    return false;
  if (packedAction == 0)
    return true;
  uint32_t const action = ActionFromPacked(packedAction);
  uint8_t const type = TypeFromPacked(packedAction);
  if (!IsValidActionValue(action))
    return false;
  return IsKnownType(type);
}


bool TryParseSetActionButtonCmsg(WorldPacket &packet, SetActionButtonCmsg &out) {
  size_t const start = packet.GetReadPos();
  size_t const remaining = packet.Size() - start;
  if (remaining < 5)
    return false;

  // AzerothCore / common: uint8 button, then uint32 packed action.
  packet.SetReadPos(start);
  {
    uint8_t const index = packet.Read<uint8_t>();
    uint32_t const packed = packet.Read<uint32_t>();
    if (TryAssignParsed(out, index, packed))
      return true;
  }

  // Trinity Cataclysm packet class: uint32 packed action, then uint8 button index.
  packet.SetReadPos(start);
  {
    uint32_t const packed = packet.Read<uint32_t>();
    uint8_t const index = packet.Read<uint8_t>();
    if (TryAssignParsed(out, index, packed))
      return true;
  }

  // Pre-WotLK wire: button, u16 action, misc, action_type (5 bytes).
  packet.SetReadPos(start);
  {
    uint8_t const index = packet.Read<uint8_t>();
    uint16_t const action16 = packet.Read<uint16_t>();
    (void)packet.Read<uint8_t>(); // misc
    uint8_t const type = packet.Read<uint8_t>();
    uint32_t const packed = Pack(action16, type);
    if (TryAssignParsed(out, index, packed))
      return true;
  }

  // Later clients: uint64 packed action, then uint8 index.
  if (remaining >= 9) {
    packet.SetReadPos(start);
    uint64_t const wide = packet.Read<uint64_t>();
    uint8_t const index = packet.Read<uint8_t>();
    if (TryAssignParsed(out, index, PackFromClientAction(static_cast<uint32_t>(wide))))
      return true;
  }

  return false;
}

bool MayPlaceOnBar(uint8_t type, uint32_t action,
                   std::unordered_set<uint32_t> const &knownSpellIds) {
  if (!IsValidActionValue(action))
    return false;
  switch (type) {
  case Spell:
    return knownSpellIds.count(action) != 0;
  case Click:
  case Macro:
  case ClickMacro:
  case Item:
    return true;
  default:
    return false;
  }
}

void ClearBar(PackedActionBar &packed) { packed.fill(0); }

} // namespace Firelands::ActionButton
