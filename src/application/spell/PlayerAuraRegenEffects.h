#pragma once

#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Aura.h>
#include <shared/game/PlayerResourceRegen.h>
#include <array>
#include <unordered_set>
#include <vector>

namespace Firelands {

/// Aggregates regen-related aura rows (Troll Regeneration, etc.) from active + known passives.
ResourceRegenModifiers ComputePlayerResourceRegenModifiers(
    std::vector<Aura> const &auras, ISpellDefinitionStore const *spellDefinitions,
    uint8 casterLevel);

void MergePermanentPassiveRegenModifiers(
    std::vector<uint32_t> const &passiveSpellIds,
    std::unordered_set<uint32_t> const &activeAuraSpellIds,
    ISpellDefinitionStore const *spellDefinitions, uint8 casterLevel,
    ResourceRegenModifiers &modifiers);

} // namespace Firelands
