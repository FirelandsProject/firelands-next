#include <application/world/PlayerPhaseShift.h>

#include <domain/models/SpellDefinition.h>
#include <shared/game/SpellAuraTypes.h>

namespace Firelands {

namespace {

void ApplyAuraRowToPlayerPhase(SpellAuraEffectRow const &row, PhaseShift &shift,
                              std::function<std::vector<uint16>(uint32)> const
                                  &resolvePhaseGroup) {
  if (row.auraType == kSpellAuraPhase) {
    uint32 const raw =
        row.miscValueB != 0u ? row.miscValueB : static_cast<uint32>(row.miscValue);
    if (raw != 0u && raw <= 0xFFFFu)
      shift.AddPhase(static_cast<uint16>(raw));
    return;
  }
  if (row.auraType == kSpellAuraPhaseGroup) {
    if (row.miscValueB != 0 && resolvePhaseGroup) {
      for (uint16 const id : resolvePhaseGroup(row.miscValueB))
        shift.AddPhase(id);
    }
    return;
  }
  if (row.auraType == kSpellAuraPhaseAlwaysVisible) {
    shift.flags |= static_cast<uint32>(PhaseShiftFlags::AlwaysVisible);
    shift.flags |= static_cast<uint32>(PhaseShiftFlags::Unphased);
  }
}

void FinalizePlayerUnphasedFlag(PhaseShift &shift) {
  bool hasNonDefault = false;
  for (PhaseRef const &phase : shift.phases) {
    if (phase.id != 0 && phase.id != kDefaultPhaseId) {
      hasNonDefault = true;
      break;
    }
  }
  if (hasNonDefault) {
    shift.flags &= ~static_cast<uint32>(PhaseShiftFlags::Unphased);
    shift.flags &= ~static_cast<uint32>(PhaseShiftFlags::InverseUnphased);
  } else if ((shift.flags & static_cast<uint32>(PhaseShiftFlags::AlwaysVisible)) ==
             0u) {
    shift.flags |= static_cast<uint32>(PhaseShiftFlags::Unphased);
    shift.flags &= ~static_cast<uint32>(PhaseShiftFlags::InverseUnphased);
  }
}

} // namespace

PhaseShift BuildPlayerPhaseShift(
    std::vector<uint16> const &areaPhaseIds, std::vector<Aura> const &auras,
    ISpellDefinitionStore const *spellDefinitions,
    std::function<std::vector<uint16>(uint32 phaseGroupId)> const &resolvePhaseGroup) {
  PhaseShift shift;
  shift.flags = static_cast<uint32>(PhaseShiftFlags::Unphased);

  for (uint16 const areaPhase : areaPhaseIds)
    shift.AddPhase(areaPhase);

  if (!spellDefinitions) {
    FinalizePlayerUnphasedFlag(shift);
    return shift;
  }

  for (Aura const &aura : auras) {
    std::optional<SpellDefinition> def =
        spellDefinitions->GetDefinition(aura.GetSpellId());
    if (!def)
      continue;
    for (SpellAuraEffectRow const &row : def->auraEffects)
      ApplyAuraRowToPlayerPhase(row, shift, resolvePhaseGroup);
  }

  FinalizePlayerUnphasedFlag(shift);
  return shift;
}

} // namespace Firelands
