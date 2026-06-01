#include <application/spell/PlayerAuraStatEffects.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/game/SpellEffectMagnitude.h>
#include <shared/game/UnitCombatStats.h>

namespace Firelands {

namespace {

constexpr int kMaxStatIndex = 4;

void ApplyResistanceRowToBonus(SpellAuraEffectRow const &row, uint8 casterLevel,
                               PlayerAuraStatBonus &bonus) {
  int32 const magnitude = SpellEffectMagnitude::NeutralMagnitudeAtLevel(
      row.basePoints, row.dieSides, row.realPointsPerLevel, casterLevel);
  if (magnitude == 0)
    return;
  int const mask = row.miscValue;
  for (uint8 school = 0; school < 7; ++school) {
    if (mask != 0 && (mask & (1 << school)) == 0)
      continue;
    if (magnitude > 0)
      bonus.resistanceBuffPos[school] += magnitude;
    else
      bonus.resistanceBuffNeg[school] += -magnitude;
  }
}

void ApplyDamageDoneRowToBonus(SpellAuraEffectRow const &row, uint8 casterLevel,
                               PlayerAuraStatBonus &bonus) {
  int32 const magnitude = SpellEffectMagnitude::NeutralMagnitudeAtLevel(
      row.basePoints, row.dieSides, row.realPointsPerLevel, casterLevel);
  if (magnitude == 0)
    return;
  int const mask = row.miscValue;
  for (uint8 school = 0; school < 7; ++school) {
    if (mask != 0 && (mask & (1 << school)) == 0)
      continue;
    if (magnitude > 0)
      bonus.damageDonePos[school] += magnitude;
    else
      bonus.damageDoneNeg[school] += -magnitude;
  }
}

void ApplyDamagePercentRowToBonus(SpellAuraEffectRow const &row, uint8 casterLevel,
                                  PlayerAuraStatBonus &bonus) {
  int32 const magnitude = SpellEffectMagnitude::NeutralMagnitudeAtLevel(
      row.basePoints, row.dieSides, row.realPointsPerLevel, casterLevel);
  if (magnitude == 0)
    return;
  float const factor = 1.f + static_cast<float>(magnitude) / 100.f;
  int const mask = row.miscValue;
  for (uint8 school = 0; school < 7; ++school) {
    if (mask != 0 && (mask & (1 << school)) == 0)
      continue;
    if (bonus.damageDonePctMultiplier[school] <= 0.f)
      bonus.damageDonePctMultiplier[school] = factor;
    else
      bonus.damageDonePctMultiplier[school] *= factor;
  }
}

void ApplyAuraRowToBonus(SpellDefinition const &def, SpellAuraEffectRow const &row,
                         uint8 casterLevel, std::array<uint32, 5> const *primaryStats,
                         PlayerAuraStatBonus &bonus) {
  uint8 const level = casterLevel > 0 ? casterLevel : 1;
  int32 const magnitude = SpellEffectMagnitude::NeutralMagnitudeAtLevel(
      row.basePoints, row.dieSides, row.realPointsPerLevel, level);

  if (row.auraType == kSpellAuraModStat) {
    int const statIdx = row.miscValue;
    if (statIdx >= 0 && statIdx <= kMaxStatIndex) {
      if (magnitude >= 0)
        bonus.posStat[static_cast<size_t>(statIdx)] += magnitude;
      else
        bonus.negStat[static_cast<size_t>(statIdx)] += -magnitude;
    }
    return;
  }

  if (row.auraType == kSpellAuraModPercentStat) {
    int const statIdx = row.miscValue;
    if (statIdx >= 0 && statIdx <= kMaxStatIndex && magnitude != 0) {
      int32 flat = magnitude;
      if (primaryStats) {
        flat = static_cast<int32>(
            static_cast<int64>((*primaryStats)[static_cast<size_t>(statIdx)]) *
            static_cast<int64>(magnitude) / 100);
      }
      if (flat >= 0)
        bonus.posStat[static_cast<size_t>(statIdx)] += flat;
      else
        bonus.negStat[static_cast<size_t>(statIdx)] += -flat;
    }
    return;
  }

  if (row.auraType == kSpellAuraModDodgePercent) {
    if (magnitude != 0)
      bonus.dodgePctBonus += static_cast<float>(magnitude);
    return;
  }

  if (row.auraType == kSpellAuraModIncreaseHealthPercent) {
    if (magnitude != 0)
      bonus.healthPctBonus += static_cast<float>(magnitude) / 100.f;
    return;
  }

  if (row.auraType == kSpellAuraModRating) {
    int const ratingIdx = row.miscValue;
    if (ratingIdx >= 0 && ratingIdx < static_cast<int>(bonus.combatRating.size()))
      bonus.combatRating[static_cast<size_t>(ratingIdx)] += magnitude;
    return;
  }

  if (row.auraType == kSpellAuraModAttackPower) {
    if (magnitude >= 0)
      bonus.attackPowerModPos += magnitude;
    else
      bonus.attackPowerModNeg += -magnitude;
    return;
  }

  if (row.auraType == kSpellAuraModAttackPowerPct) {
    if (magnitude != 0)
      bonus.attackPowerMultiplier += static_cast<float>(magnitude) / 100.0f;
    return;
  }

  if (row.auraType == kSpellAuraModResistance) {
    ApplyResistanceRowToBonus(row, level, bonus);
    return;
  }

  if (row.auraType == kSpellAuraModDamageDone) {
    ApplyDamageDoneRowToBonus(row, level, bonus);
    return;
  }

  if (row.auraType == kSpellAuraModDamagePercentDone) {
    ApplyDamagePercentRowToBonus(row, level, bonus);
    return;
  }

  if (row.auraType == kSpellAuraModCombatSpeedPct) {
    if (magnitude != 0) {
      float const factor = 1.f + static_cast<float>(magnitude) / 100.f;
      bonus.meleeHasteMultiplier *= factor;
      bonus.rangedHasteMultiplier *= factor;
      bonus.castHasteMultiplier *= factor;
    }
  }
}

void ApplyDefinitionAurasToBonus(SpellDefinition const &def, uint8 casterLevel,
                                 std::array<uint32, 5> const *primaryStats,
                                 PlayerAuraStatBonus &bonus) {
  if (!def.auraEffects.empty()) {
    for (SpellAuraEffectRow const &row : def.auraEffects)
      ApplyAuraRowToBonus(def, row, casterLevel, primaryStats, bonus);
    return;
  }
  if (!def.hasAuraEffect)
    return;
  SpellAuraEffectRow row{};
  row.effectIndex = def.auraEffectIndex;
  row.auraType = def.auraEffectType;
  row.basePoints = def.auraBasePoints;
  row.dieSides = def.auraDieSides;
  row.realPointsPerLevel = def.auraRealPointsPerLevel;
  ApplyAuraRowToBonus(def, row, casterLevel, primaryStats, bonus);
}

} // namespace

PlayerAuraStatBonus ComputePlayerAuraStatBonus(
    std::vector<Aura> const &auras, ISpellDefinitionStore const *spellDefinitions,
    uint8 casterLevel, std::array<uint32, 5> const *primaryStats) {
  PlayerAuraStatBonus bonus{};
  if (!spellDefinitions)
    return bonus;

  for (Aura const &aura : auras) {
    std::optional<SpellDefinition> def =
        spellDefinitions->GetDefinition(aura.GetSpellId());
    if (!def)
      continue;
    uint8 const level = aura.GetClientWireMeta().casterLevel > 0
                            ? aura.GetClientWireMeta().casterLevel
                            : casterLevel;
    ApplyDefinitionAurasToBonus(*def, level, primaryStats, bonus);
  }
  return bonus;
}

void MergePermanentPassiveSpellBonuses(
    std::vector<uint32_t> const &passiveSpellIds,
    std::unordered_set<uint32_t> const &activeAuraSpellIds,
    ISpellDefinitionStore const *spellDefinitions, uint8 casterLevel,
    std::array<uint32, 5> const *primaryStats, PlayerAuraStatBonus &bonus) {
  if (!spellDefinitions)
    return;
  for (uint32_t const spellId : passiveSpellIds) {
    if (spellId == 0u || activeAuraSpellIds.count(spellId) != 0)
      continue;
    std::optional<SpellDefinition> def = spellDefinitions->GetDefinition(spellId);
    if (!def || !def->isAlwaysOnLoginPassiveSpell())
      continue;
    ApplyDefinitionAurasToBonus(*def, casterLevel, primaryStats, bonus);
  }
}

void ApplyPlayerAuraStatBonusToCombatStats(UnitCombatStats &stats,
                                           PlayerAuraStatBonus const &bonus) {
  ApplyAuraStatBonusToCombatStats(stats, bonus.attackPowerModPos, bonus.attackPowerModNeg,
                                  bonus.attackPowerMultiplier);
  for (size_t school = 0; school < stats.spellDamageDonePos.size(); ++school) {
    stats.spellDamageDonePos[school] += bonus.damageDonePos[school];
    stats.spellDamageDonePos[school] -= bonus.damageDoneNeg[school];
    stats.resistanceBuffPos[school] = bonus.resistanceBuffPos[school];
    stats.resistanceBuffNeg[school] = bonus.resistanceBuffNeg[school];
  }
}

} // namespace Firelands
