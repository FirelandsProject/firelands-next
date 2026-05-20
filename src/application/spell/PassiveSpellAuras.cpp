#include <application/spell/PassiveSpellAuras.h>
#include <application/spell/SpellHitEffects.h>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/StarterSpellFilters.h>

namespace Firelands {

namespace {

bool ShouldApplyPassiveAura(SpellDefinition const &def) {
  if (!def.isPassiveSpell() || !def.hasAuraEffect)
    return false;
  if (IsExcludedLoginAuraType(def.auraEffectType))
    return false;
  return true;
}

} // namespace

/// Builds login aura outcomes for racial passives only (`candidateSpellIds` should be
/// `PlayerCreateInfoService::GetRacialSpells`, not the full spellbook).
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
    if (!def || !ShouldApplyPassiveAura(*def))
      continue;

    SpellCastOutcome outcome{};
    SpellHitEffects::ApplyAuraFromDefinition(&*def, unitGuid, unitGuid, casterLevel,
                                             now, castTables, &outcome);
    if (outcome.hasAuraApply)
      outcomes.push_back(outcome);
  }
  return outcomes;
}

} // namespace Firelands
