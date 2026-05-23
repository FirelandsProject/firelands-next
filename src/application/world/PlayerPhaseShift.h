#pragma once

#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Aura.h>
#include <shared/game/PhaseShift.h>

#include <functional>
#include <vector>

namespace Firelands {

/// Area phases (`phase_area`) first, then aura-driven phases (TrinityCore `OnAreaChange` + auras).
PhaseShift BuildPlayerPhaseShift(
    std::vector<uint16> const &areaPhaseIds, std::vector<Aura> const &auras,
    ISpellDefinitionStore const *spellDefinitions,
    std::function<std::vector<uint16>(uint32 phaseGroupId)> const &resolvePhaseGroup);

} // namespace Firelands
