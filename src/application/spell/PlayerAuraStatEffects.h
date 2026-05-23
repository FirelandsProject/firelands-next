#pragma once

#include <array>
#include <cstdint>
#include <unordered_set>
#include <vector>
#include <domain/world/Aura.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/game/UnitCombatStats.h>
#include <vector>

namespace Firelands {

/// Aggregated stat/rating/AP bonuses from active auras (update fields on apply/remove).
struct PlayerAuraStatBonus {
  std::array<int32, 5> posStat{};
  std::array<int32, 5> negStat{};
  /// `PLAYER_FIELD_COMBAT_RATING_1` + index (0..25).
  std::array<int32, 26> combatRating{};
  /// `UNIT_FIELD_RESISTANCEBUFFMODS*` per school (0..6).
  std::array<int32, 7> resistanceBuffPos{};
  std::array<int32, 7> resistanceBuffNeg{};
  /// Flat `PLAYER_FIELD_MOD_DAMAGE_DONE_POS` per school.
  std::array<int32, 7> damageDonePos{};
  std::array<int32, 7> damageDoneNeg{};
  /// Multiplier on baseline 1.0 (`PLAYER_FIELD_MOD_DAMAGE_DONE_PCT`), multiplicative stack.
  std::array<float, 7> damageDonePctMultiplier{};
  /// Additive percent points for `PLAYER_DODGE_PERCENTAGE` (Night Elf Quickness, …).
  float dodgePctBonus = 0.f;
  /// Additive fraction on baseline max health (Tauren Endurance, …).
  float healthPctBonus = 0.f;
  int32 attackPowerModPos = 0;
  int32 attackPowerModNeg = 0;
  /// Fraction added to melee AP (e.g. 0.07f = 7% from `SPELL_AURA_MOD_ATTACK_POWER_PCT`).
  float attackPowerMultiplier = 0.f;
};

/// Sums bonuses from `auras` using `spellDefinitions` aura rows (`auraEffects` when present).
PlayerAuraStatBonus ComputePlayerAuraStatBonus(
    std::vector<Aura> const &auras, ISpellDefinitionStore const *spellDefinitions,
    uint8 casterLevel, std::array<uint32, 5> const *primaryStats = nullptr);

/// Adds bonuses from permanent passives that are known but not yet on the unit aura list.
void MergePermanentPassiveSpellBonuses(
    std::vector<uint32_t> const &passiveSpellIds,
    std::unordered_set<uint32_t> const &activeAuraSpellIds,
    ISpellDefinitionStore const *spellDefinitions, uint8 casterLevel,
    std::array<uint32, 5> const *primaryStats, PlayerAuraStatBonus &bonus);

/// Merges aura bonuses into live combat stats (AP, spell damage, resistance buffs).
void ApplyPlayerAuraStatBonusToCombatStats(UnitCombatStats &stats,
                                           PlayerAuraStatBonus const &bonus);

} // namespace Firelands
