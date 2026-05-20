#pragma once

#include <application/spell/SpellManager.h>
#include <chrono>
#include <cstdint>
#include <vector>

namespace Firelands {

class ISpellCastTables;
class ISpellDefinitionStore;

/// Builds `SpellCastOutcome` aura applies for racial passive spells in `candidateSpellIds`.
/// Pass only `GetRacialSpells` ids — not the full spellbook. Skips mount/form auras.
std::vector<SpellCastOutcome> BuildPassiveAuraOutcomes(
    uint64_t unitGuid, uint8_t casterLevel,
    std::vector<uint32_t> const &candidateSpellIds,
    ISpellDefinitionStore const *spellDefinitions,
    ISpellCastTables const *castTables,
    std::chrono::steady_clock::time_point now);

} // namespace Firelands
