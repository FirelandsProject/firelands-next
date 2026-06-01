#include <application/spell/PassiveSpellAuras.h>
#include <application/spell/SpellHitEffects.h>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/StarterSpellFilters.h>

#include <algorithm>

namespace Firelands {

namespace {

bool ShouldApplyPassiveAuraRow(SpellDefinition const &def, SpellAuraEffectRow const &row) {
  if (!def.isAlwaysOnLoginPassiveSpell())
    return false;
  if (IsExcludedLoginAuraType(row.auraType))
    return false;
  return true;
}

bool SpellQualifiesAsLoginPassive(SpellDefinition const &def) {
  if (!def.isAlwaysOnLoginPassiveSpell())
    return false;
  if (!def.auraEffects.empty()) {
    for (SpellAuraEffectRow const &row : def.auraEffects) {
      if (!IsExcludedLoginAuraType(row.auraType))
        return true;
    }
    return false;
  }
  return def.hasAuraEffect && !IsExcludedLoginAuraType(def.auraEffectType);
}

} // namespace

std::vector<uint32_t> CollectLoginPassiveSpellIds(
    std::unordered_set<uint32_t> const &knownSpellIds,
    ISpellDefinitionStore const *spellDefinitions) {
  std::vector<uint32_t> out;
  if (!spellDefinitions || knownSpellIds.empty())
    return out;

  out.reserve(knownSpellIds.size() / 4u + 4u);
  for (uint32_t const spellId : knownSpellIds) {
    if (spellId == 0u || IsLanguagePassiveSpell(spellId))
      continue;
    if (IsRidingOrTransportStarterSpell(spellId) ||
        IsClassShapeshiftStarterSpell(spellId))
      continue;

    std::optional<SpellDefinition> def = spellDefinitions->GetDefinition(spellId);
    if (!def || !SpellQualifiesAsLoginPassive(*def))
      continue;
    out.push_back(spellId);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::vector<SpellCastOutcome> BuildPassiveAuraOutcomes(
    uint64_t unitGuid, uint8_t casterLevel,
    std::vector<uint32_t> const &candidateSpellIds,
    ISpellDefinitionStore const *spellDefinitions,
    ISpellCastTables const *castTables,
    std::chrono::steady_clock::time_point now) {
  std::vector<SpellCastOutcome> outcomes;
  if (unitGuid == 0u || !spellDefinitions)
    return outcomes;

  for (uint32_t const spellId : candidateSpellIds) {
    if (spellId == 0u || IsLanguagePassiveSpell(spellId))
      continue;
    if (IsRidingOrTransportStarterSpell(spellId) ||
        IsClassShapeshiftStarterSpell(spellId))
      continue;

    std::optional<SpellDefinition> def = spellDefinitions->GetDefinition(spellId);
    if (!def || !def->isAlwaysOnLoginPassiveSpell())
      continue;

    if (!def->auraEffects.empty()) {
      bool hasApplicable = false;
      for (SpellAuraEffectRow const &row : def->auraEffects) {
        if (ShouldApplyPassiveAuraRow(*def, row)) {
          hasApplicable = true;
          break;
        }
      }
      if (!hasApplicable)
        continue;
      SpellCastOutcome outcome{};
      SpellHitEffects::ApplyAuraFromDefinition(&*def, unitGuid, unitGuid, casterLevel,
                                               now, castTables, &outcome);
      if (outcome.hasAuraApply)
        outcomes.push_back(outcome);
      continue;
    }

    if (!def->hasAuraEffect || IsExcludedLoginAuraType(def->auraEffectType))
      continue;

    SpellCastOutcome outcome{};
    SpellHitEffects::ApplyAuraFromDefinition(&*def, unitGuid, unitGuid, casterLevel, now,
                                             castTables, &outcome);
    if (outcome.hasAuraApply)
      outcomes.push_back(outcome);
  }
  return outcomes;
}

} // namespace Firelands
