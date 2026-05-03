#include <application/spell/SpellManager.h>

#include <algorithm>
#include <cmath>

namespace Firelands {

SpellManager::SpellManager(std::shared_ptr<ISpellDefinitionStore const> spellDefinitions,
                           std::shared_ptr<ISpellCastTables const> spellCastTables)
    : m_spellDefinitions(std::move(spellDefinitions)),
      m_spellCastTables(std::move(spellCastTables)) {}

bool SpellManager::IsSpellKnown(uint32 spellId,
                                std::vector<uint32> const *knownSpells) {
  if (!knownSpells || spellId == 0u)
    return false;
  // Hot path: linear scan on a small list (~100–200). For higher load, replace
  // `_knownSpells` storage with a sorted vector + binary_search or a flat_hash_set.
  auto const &v = *knownSpells;
  return std::find(v.begin(), v.end(), spellId) != v.end();
}

void SpellManager::ProcessCastRequest(SpellCastRequest const &req,
                                      SpellCastOutcome *out) const {
  if (!out)
    return;
  out->kind = SpellCastOutcome::Kind::None;

  uint32 const spellId = static_cast<uint32>(req.client.spellId);
  if (!IsSpellKnown(spellId, req.knownSpells)) {
    SpellCastWire::BuildSpellFailure(
        out->failurePacket, req.casterGuid, req.client.castId, req.client.spellId,
        SpellCastWire::SPELL_FAILED_SPELL_UNAVAILABLE);
    out->kind = SpellCastOutcome::Kind::SpellFailure;
    return;
  }

  if (m_spellDefinitions && !m_spellDefinitions->HasSpell(spellId)) {
    SpellCastWire::BuildSpellFailure(
        out->failurePacket, req.casterGuid, req.client.castId, req.client.spellId,
        SpellCastWire::SPELL_FAILED_SPELL_UNAVAILABLE);
    out->kind = SpellCastOutcome::Kind::SpellFailure;
    return;
  }

  if (req.now < req.gcdReady) {
    SpellCastWire::BuildSpellFailure(out->failurePacket, req.casterGuid,
                                     req.client.castId, req.client.spellId,
                                     SpellCastWire::SPELL_FAILED_NOT_READY);
    out->kind = SpellCastOutcome::Kind::SpellFailure;
    return;
  }

  if (m_spellDefinitions && m_spellCastTables) {
    if (auto const def = m_spellDefinitions->GetDefinition(spellId)) {
      float const maxYards =
          m_spellCastTables->GetHostileRangeMaxYards(def->rangeIndex);
      if (maxYards > 0.f && req.hasCasterWorldPosition && req.hasTargetWorldPosition) {
        float const dx = req.targetX - req.casterX;
        float const dy = req.targetY - req.casterY;
        float const dz = req.targetZ - req.casterZ;
        float const dist =
            std::sqrt(dx * dx + dy * dy + dz * dz);
        // TCPP `MAX_SPELL_RANGE_TOLERANCE` (yards) — small slack so borderline casts match client.
        constexpr float kRangeToleranceYards = 3.0f;
        if (dist > maxYards + kRangeToleranceYards) {
          SpellCastWire::BuildSpellFailure(
              out->failurePacket, req.casterGuid, req.client.castId, req.client.spellId,
              SpellCastWire::SPELL_FAILED_OUT_OF_RANGE);
          out->kind = SpellCastOutcome::Kind::SpellFailure;
          return;
        }
      }
    }
  }

  uint32 castTimeStart = 0;
  if (m_spellDefinitions && m_spellCastTables) {
    if (auto const def = m_spellDefinitions->GetDefinition(spellId))
      castTimeStart =
          m_spellCastTables->GetCastTimeMs(def->castingTimeIndex);
  }

  uint32 targetFlags = req.client.targetFlags;
  uint64 targetUnitGuid = req.casterGuid;
  if ((req.client.targetFlags & SpellCastWire::ClientTargetPrimaryGuidMask) == 0) {
    targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
    targetUnitGuid = req.casterGuid;
  } else {
    targetUnitGuid = req.client.unitTargetGuid != 0 ? req.client.unitTargetGuid
                                                    : req.casterGuid;
  }

  uint64 hitGuid = req.casterGuid;
  if ((req.client.targetFlags & SpellCastWire::ClientTargetPrimaryGuidMask) != 0 &&
      req.client.unitTargetGuid != 0)
    hitGuid = req.client.unitTargetGuid;

  uint32 const castFlagsStart = SpellCastWire::CAST_FLAG_HAS_TRAJECTORY;
  uint32 const castFlagsGo = SpellCastWire::CAST_FLAG_UNKNOWN_9;
  uint32 const castTimeGo = static_cast<uint32>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          req.now.time_since_epoch())
          .count());

  SpellCastWire::BuildSpellStart(out->spellStart, req.casterGuid, req.client.castId,
                                 spellId, castFlagsStart, 0, castTimeStart, targetFlags,
                                 targetUnitGuid);

  uint64 const hitTargets[1] = {hitGuid};
  SpellCastWire::BuildSpellGo(out->spellGo, req.casterGuid, req.client.castId, spellId,
                              castFlagsGo, 0, castTimeGo, hitTargets, 1, targetFlags,
                              targetUnitGuid);

  out->newGcdReady = req.now + std::chrono::milliseconds(1500);
  out->kind = SpellCastOutcome::Kind::SpellStartAndGo;
}

} // namespace Firelands
