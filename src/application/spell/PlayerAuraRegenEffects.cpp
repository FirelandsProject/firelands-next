#include <application/spell/PlayerAuraRegenEffects.h>

#include <domain/models/SpellDefinition.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/game/SpellEffectMagnitude.h>

namespace Firelands {

namespace {

void ApplyAuraRowToRegenModifiers(SpellAuraEffectRow const &row, uint8 casterLevel,
                                  ResourceRegenModifiers &modifiers) {
  int32 const magnitude = SpellEffectMagnitude::NeutralMagnitudeAtLevel(
      row.basePoints, row.dieSides, row.realPointsPerLevel, casterLevel);
  if (magnitude == 0)
    return;

  switch (row.auraType) {
  case kSpellAuraModHealthRegenPercent:
    modifiers.healthRegenPct += magnitude;
    break;
  case kSpellAuraModRegenDuringCombat:
    modifiers.healthRegenDuringCombatPct += magnitude;
    break;
  case kSpellAuraModPowerRegenPercent:
    modifiers.powerRegenPct += magnitude;
    break;
  default:
    break;
  }
}

void ApplyDefinitionRegenModifiers(SpellDefinition const &def, uint8 casterLevel,
                                   ResourceRegenModifiers &modifiers) {
  if (!def.auraEffects.empty()) {
    for (SpellAuraEffectRow const &row : def.auraEffects)
      ApplyAuraRowToRegenModifiers(row, casterLevel, modifiers);
    return;
  }
  if (!def.hasAuraEffect)
    return;
  SpellAuraEffectRow row{};
  row.auraType = def.auraEffectType;
  row.basePoints = def.auraBasePoints;
  row.dieSides = def.auraDieSides;
  row.realPointsPerLevel = def.auraRealPointsPerLevel;
  ApplyAuraRowToRegenModifiers(row, casterLevel, modifiers);
}

} // namespace

ResourceRegenModifiers ComputePlayerResourceRegenModifiers(
    std::vector<Aura> const &auras, ISpellDefinitionStore const *spellDefinitions,
    uint8 casterLevel) {
  ResourceRegenModifiers modifiers{};
  if (!spellDefinitions)
    return modifiers;

  for (Aura const &aura : auras) {
    std::optional<SpellDefinition> def =
        spellDefinitions->GetDefinition(aura.GetSpellId());
    if (!def)
      continue;
    uint8 const level = aura.GetClientWireMeta().casterLevel > 0
                            ? aura.GetClientWireMeta().casterLevel
                            : casterLevel;
    ApplyDefinitionRegenModifiers(*def, level, modifiers);
  }
  return modifiers;
}

void MergePermanentPassiveRegenModifiers(
    std::vector<uint32_t> const &passiveSpellIds,
    std::unordered_set<uint32_t> const &activeAuraSpellIds,
    ISpellDefinitionStore const *spellDefinitions, uint8 casterLevel,
    ResourceRegenModifiers &modifiers) {
  if (!spellDefinitions)
    return;
  for (uint32_t const spellId : passiveSpellIds) {
    if (spellId == 0u || activeAuraSpellIds.count(spellId) != 0)
      continue;
    std::optional<SpellDefinition> def = spellDefinitions->GetDefinition(spellId);
    if (!def || !def->isAlwaysOnLoginPassiveSpell())
      continue;
    ApplyDefinitionRegenModifiers(*def, casterLevel, modifiers);
  }
}

} // namespace Firelands
