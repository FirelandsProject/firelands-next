#pragma once

#include <domain/models/Character.h>
#include <shared/game/UnitCombatStats.h>

namespace Firelands {

bool UsesAgilityForMeleeAttackPower(uint8 classId);
int32 ComputeBaseMeleeAttackPower(Character const &character);
bool UsesBaselineSpellPowerFromIntellect(uint8 classId);
UnitCombatStats BuildPlayerCombatStats(Character const &character);

} // namespace Firelands
