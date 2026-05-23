#pragma once

#include <shared/Common.h>
#include <array>
#include <cstdint>

namespace Firelands {

/// Server-side combat snapshot mirrored from baseline update fields (armor, resist, AP, spell power).
struct UnitCombatStats {
  uint8 level = 1;
  uint32 armor = 0;
  std::array<uint32, 7> resistance{};
  /// `UNIT_FIELD_RESISTANCEBUFFMODS*` from racials/passives (ref `Unit::UpdateResistanceBuffModsMod`).
  std::array<int32, 7> resistanceBuffPos{};
  std::array<int32, 7> resistanceBuffNeg{};
  std::array<int32, 7> spellDamageDonePos{};
  int32 attackPower = 0;
  int32 attackPowerModPos = 0;
  int32 attackPowerModNeg = 0;
  float attackPowerMultiplier = 0.f;
};

int32 EffectiveAttackPower(UnitCombatStats const &stats);

uint32 ComputeBaselineArmor(uint8 classId, uint8 level, uint32 agi, uint32 str,
                            uint32 sta);

void ApplyAuraStatBonusToCombatStats(UnitCombatStats &stats,
                                     int32 attackPowerModPos, int32 attackPowerModNeg,
                                     float attackPowerMultiplier);

/// Effective magic resistance for mitigation (`base + buff pos - buff neg`).
uint32 EffectiveSchoolResistance(UnitCombatStats const &stats, uint8 school);

UnitCombatStats BuildCreatureCombatStats(uint8 level, uint8 unitClass);

} // namespace Firelands
