#pragma once

#include <shared/Common.h>
#include <shared/game/UnitCombatStats.h>
#include <cstdint>

namespace Firelands {

/// Reference `Unit::CalcArmorReducedDamage` (Cataclysm 4.3.4).
uint32 CalcArmorReducedDamage(uint8 attackerLevel, uint32 victimArmor, uint32 damage);

/// Reference `Unit::CalculateAverageResistReduction` (level-based constant, no penetration).
float CalcAverageMagicResistReduction(uint8 attackerLevel, uint8 victimLevel,
                                    uint32 victimResistance, bool holySchool);

/// Adds baseline spell power / AP before victim mitigation (simplified `SpellDamageBonusDone`).
uint32 ApplyOutgoingDamageBonuses(uint32 baseDamage, UnitCombatStats const *caster,
                                  uint32 schoolMask, bool periodicTick);

/// Melee auto-attack raw damage before armor (matches baseline AP → weapon band midpoint).
uint32 ComputeMeleeSwingRawDamage(UnitCombatStats const &attacker);

/// Full pipeline: outgoing bonuses then armor / resist on the victim. Heals pass through unchanged.
int32 ResolveMitigatedHealthDelta(int32 rawSignedDelta, uint32 schoolMask,
                                  UnitCombatStats const *casterStats,
                                  uint8 casterLevel, UnitCombatStats const *victimStats,
                                  bool periodicTick = false);

/// Melee swing damage after armor mitigation.
uint32 ComputeMeleeSwingDamage(UnitCombatStats const &attacker,
                               UnitCombatStats const &victim);

} // namespace Firelands
