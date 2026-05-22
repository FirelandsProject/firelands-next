#pragma once

#include <domain/models/Character.h>
#include <domain/models/CharacterActionButtons.h>
#include <shared/game/ActionButton.h>
#include <array>
#include <unordered_set>

namespace Firelands {

void ApplyPersistedActionButtons(ActionButton::PackedActionBar &packed,
                               CharacterActionButtonState const &state);

CharacterActionButtonState BuildPersistedActionButtons(
    ActionButton::PackedActionBar const &packed, uint8_t spec = 0);

/// Spellbook + bag-0 inventory rules for placing a button on the main action bar.
bool IsValidActionButtonPlacement(
    uint8_t type, uint32_t action,
    std::unordered_set<uint32_t> const &knownSpellIds, Character const &character);

} // namespace Firelands
