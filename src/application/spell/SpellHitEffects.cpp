#include <application/spell/SpellHitEffects.h>
#include <application/spell/SpellManager.h>
#include <shared/Logger.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/game/SpellEffectMagnitude.h>

namespace Firelands {
namespace SpellHitEffects {

namespace {

/// Old fallback when `SpellDuration.dbc` was missing; treat as "unknown" and re-resolve.
constexpr uint32 kLegacyUnknownAuraDurationMs = 3600000u;

bool AuraIsNegative(SpellDefinition const &def, uint32 auraEffectType) {
  if (auraEffectType == kSpellAuraPeriodicDamage)
    return true;
  if ((def.attributes & SpellAttr0::kNegativeSpell) != 0u)
    return true;
  if ((def.attributes & SpellAttr0::kAuraIsDebuff) != 0u)
    return true;
  return false;
}

SpellAuraEffectRow const *PickPrimaryAuraRowForCast(SpellDefinition const &def) {
  if (!def.auraEffects.empty()) {
    for (SpellAuraEffectRow const &row : def.auraEffects) {
      if (row.auraType == kSpellAuraPeriodicHeal ||
          row.auraType == kSpellAuraPeriodicDamage)
        return &row;
    }
    return &def.auraEffects.front();
  }
  if (!def.hasAuraEffect)
    return nullptr;
  return nullptr;
}

void FillAuraOutcomeFromRow(SpellDefinition const &def, SpellAuraEffectRow const &row,
                            uint64 hitGuid, uint64 casterGuid, uint8 casterLevel,
                            ISpellCastTables const *castTables, SpellCastOutcome *out) {
  if (!out)
    return;

  uint32 durationMs = 0;
  if (castTables) {
    uint32 const idx =
        def.auraDurationIndex != 0u ? def.auraDurationIndex : def.durationIndex;
    durationMs = castTables->GetDurationMs(idx, casterLevel);
  }
  uint8 const level = casterLevel > 0 ? casterLevel : 1;
  out->hasAuraApply = true;
  out->auraTargetGuid = hitGuid;
  out->auraCasterGuid = casterGuid;
  out->auraSpellId = def.id;
  out->auraEffectType = row.auraType;
  out->auraEffectIndex = row.effectIndex;
  out->auraBasePoints = row.basePoints;
  out->auraDieSides = row.dieSides;
  out->auraDurationMs =
      ResolveAuraDurationMs(def.id, level, durationMs, &def, castTables);
  out->auraPeriodicPeriodMs = row.periodMs;
  if (row.auraType == kSpellAuraPeriodicHeal) {
    out->auraPeriodicHealthDeltaPerTick =
        SpellEffectMagnitude::PeriodicHealTickAtLevel(
            row.basePoints, row.dieSides, row.realPointsPerLevel, level);
  } else if (row.auraType == kSpellAuraPeriodicDamage) {
    out->auraPeriodicHealthDeltaPerTick =
        SpellEffectMagnitude::PeriodicDamageTickAtLevel(
            row.basePoints, row.dieSides, row.realPointsPerLevel, level);
  } else {
    out->auraPeriodicHealthDeltaPerTick = row.periodicHealthDeltaPerTick;
  }
  out->auraIsNegative = AuraIsNegative(def, row.auraType);
  out->auraCasterLevel = level;
}

int32 SumImmediateHealthFromEffectRows(SpellDefinition const &def, uint8 casterLevel) {
  if (def.effectRows.empty())
    return 0;
  uint8 const level = casterLevel > 0 ? casterLevel : 1;
  int32 sum = 0;
  for (SpellEffectRow const &row : def.effectRows) {
    if (!SpellEffectKindHasImmediateHealthDelta(row.effectKind))
      continue;
    sum += SpellEffectMagnitude::SignedImmediateHealthDeltaAtLevel(
        row.effectKind, row.basePoints, row.dieSides, row.realPointsPerLevel, level);
  }
  return sum;
}

} // namespace

uint64 ResolvePrimarySpellHitUnitGuid(uint32 clientTargetFlags, uint64 casterGuid,
                                      uint64 unitTargetGuid) {
  if ((clientTargetFlags & SpellCastWire::ClientTargetPrimaryGuidMask) != 0 &&
      unitTargetGuid != 0u)
    return unitTargetGuid;
  return casterGuid;
}

void ApplyImmediateHealthFromDefinition(SpellDefinition const *def, uint64 hitGuid,
                                        SpellCastOutcome *out) {
  if (!out || !def)
    return;
  int32 delta = SumImmediateHealthFromEffectRows(*def, out->auraCasterLevel);
  if (delta == 0)
    delta = def->immediateHealthEffectDelta;
  if (delta == 0)
    return;
  out->hasDirectHealthEffect = true;
  out->directHealthTargetGuid = hitGuid;
  out->directHealthDelta = delta;
  out->directHealthSchoolMask = def->schoolMask;
}

void ApplyAuraFromRow(SpellDefinition const &def, SpellAuraEffectRow const &row,
                      uint64 hitGuid, uint64 casterGuid, uint8 casterLevel,
                      std::chrono::steady_clock::time_point now,
                      ISpellCastTables const *castTables, SpellCastOutcome *out) {
  (void)now;
  FillAuraOutcomeFromRow(def, row, hitGuid, casterGuid, casterLevel, castTables, out);
}

void ApplyAuraFromDefinition(SpellDefinition const *def, uint64 hitGuid, uint64 casterGuid,
                             uint8 casterLevel, std::chrono::steady_clock::time_point now,
                             ISpellCastTables const *castTables, SpellCastOutcome *out) {
  if (!out || !def)
    return;

  if (SpellAuraEffectRow const *row = PickPrimaryAuraRowForCast(*def)) {
    ApplyAuraFromRow(*def, *row, hitGuid, casterGuid, casterLevel, now, castTables, out);
    return;
  }

  if (!def->hasAuraEffect)
    return;

  SpellAuraEffectRow row{};
  row.effectIndex = def->auraEffectIndex;
  row.auraType = def->auraEffectType;
  row.basePoints = def->auraBasePoints;
  row.dieSides = def->auraDieSides;
  row.realPointsPerLevel = def->auraRealPointsPerLevel;
  row.periodMs = def->auraPeriodicPeriodMs;
  row.periodicHealthDeltaPerTick = def->auraPeriodicHealthDeltaPerTick;
  FillAuraOutcomeFromRow(*def, row, hitGuid, casterGuid, casterLevel, castTables, out);
  (void)now;
}

void ApplySpellEffectsFromDefinition(SpellDefinition const *def, uint64 hitGuid,
                                     uint64 casterGuid, uint8 casterLevel,
                                     std::chrono::steady_clock::time_point now,
                                     ISpellCastTables const *castTables,
                                     SpellCastOutcome *out) {
  if (!out || !def)
    return;

  out->auraCasterLevel = casterLevel > 0 ? casterLevel : 1;

  ApplyImmediateHealthFromDefinition(def, hitGuid, out);

  if (!def->effectRows.empty()) {
    uint8 const level = out->auraCasterLevel;
    for (SpellEffectRow const &row : def->effectRows) {
      if (row.effectKind == kSpellEffectEnergize) {
        int32 const amount = SpellEffectMagnitude::NeutralMagnitudeAtLevel(
            row.basePoints, row.dieSides, row.realPointsPerLevel, level);
        if (amount != 0)
          out->power1Delta += amount;
      }
    }
  }

  ApplyAuraFromDefinition(def, hitGuid, casterGuid, casterLevel, now, castTables, out);
}

uint32 ResolveAuraDurationMs(uint32 spellId, uint8 casterLevel, uint32 outcomeDurationMs,
                             SpellDefinition const *def,
                             ISpellCastTables const *castTables) {
  if (outcomeDurationMs > 0u && outcomeDurationMs != kLegacyUnknownAuraDurationMs)
    return outcomeDurationMs;

  if (!def || !castTables)
    return outcomeDurationMs;

  uint32 const idx =
      def->auraDurationIndex != 0u ? def->auraDurationIndex : def->durationIndex;
  if (idx == 0u)
    return 0u;

  uint32 const resolved = castTables->GetDurationMs(idx, casterLevel);
  if (resolved == 0u) {
    LOG_WARN("Spell {}: duration index {} not found in SpellDuration.dbc; aura timer "
             "may desync client",
             spellId, idx);
    return 0u;
  }
  return resolved;
}

} // namespace SpellHitEffects
} // namespace Firelands
