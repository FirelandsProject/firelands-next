#include <application/services/CharacterActionButtons.h>

namespace Firelands {

void ApplyPersistedActionButtons(ActionButton::PackedActionBar &packed,
                               CharacterActionButtonState const &state) {
  ActionButton::ClearBar(packed);
  for (PersistedActionButton const &row : state.buttons) {
    if (!ActionButton::IsValidButtonIndex(row.button))
      continue;
    if (!ActionButton::IsValidActionValue(row.action) ||
        !ActionButton::IsKnownType(row.type))
      continue;
    packed[row.button] = ActionButton::PackWire(row.action, row.type);
  }
}

CharacterActionButtonState BuildPersistedActionButtons(
    ActionButton::PackedActionBar const &packed, uint8_t spec) {
  (void)spec;
  CharacterActionButtonState state;
  state.buttons.reserve(ActionButton::kMaxButtons);
  for (size_t i = 0; i < ActionButton::kMaxButtons; ++i) {
    uint32_t const wire = packed[i];
    if (wire == 0)
      continue;
    uint32_t const packed32 = ActionButton::PackFromClientAction(wire);
    uint32_t const action = ActionButton::ActionFromPacked(packed32);
    uint8_t const type = ActionButton::TypeFromPacked(packed32);
    if (!ActionButton::IsValidActionValue(action) || !ActionButton::IsKnownType(type))
      continue;
    state.buttons.push_back(
        PersistedActionButton{static_cast<uint8_t>(i), action, type});
  }
  return state;
}

bool IsValidActionButtonPlacement(
    uint8_t type, uint32_t action,
    std::unordered_set<uint32_t> const &knownSpellIds, Character const &character) {
  if (!ActionButton::IsValidActionValue(action))
    return false;
  switch (type) {
  case ActionButton::Spell:
    return knownSpellIds.count(action) != 0;
  case ActionButton::Item:
    return character.HasBag0ItemEntry(action);
  case ActionButton::Click:
  case ActionButton::Macro:
  case ActionButton::ClickMacro:
    return action != 0;
  default:
    return false;
  }
}

} // namespace Firelands
