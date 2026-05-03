#include <application/spell/SpellManager.h>

#include <application/spell/SpellHitEffects.h>
#include <application/ports/IMapCollisionQueries.h>
#include <shared/game/SpellAttributes.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace Firelands {

namespace {

/// After self-target handling: infer helpful vs harmful spell for `SpellRange.dbc` band selection.
static bool SpellTreatAsBeneficialForFriendlySpellRange(SpellDefinition const &def) {
  if (def.immediateHealthEffectDelta > 0)
    return true;
  if (def.immediateHealthEffectDelta < 0)
    return false;
  if ((def.attributes & SpellAttr0::kNegativeSpell) != 0u)
    return false;
  if ((def.attributes & SpellAttr0::kAuraIsDebuff) != 0u)
    return false;
  if ((def.attributesEx & SpellAttrEx::kInitiatesCombat) != 0u)
    return false;
  if (def.spellEffectHasHarmKind)
    return false;
  return true;
}

/// Picks `SpellRange.dbc` hostile vs friendly bands (index 0 vs 1). Self-casts always use friendly.
bool SpellUsesFriendlySpellRangeColumns(SpellDefinition const *def,
                                          SpellCastRequest const &req) {
  bool const targetIsSelf =
      req.client.unitTargetGuid == 0 ||
      req.client.unitTargetGuid == req.casterGuid;
  if (!def)
    return targetIsSelf;
  if (targetIsSelf)
    return true;
  bool const beneficialSpell = SpellTreatAsBeneficialForFriendlySpellRange(*def);
  if (!beneficialSpell)
    return false;
  if (req.hasTargetFactionReactionHint)
    return req.targetIsFriendlyTeamForSpellRange;
  return true;
}

} // namespace

SpellManager::SpellManager(std::shared_ptr<ISpellDefinitionStore const> spellDefinitions,
                           std::shared_ptr<ISpellCastTables const> spellCastTables)
    : m_spellDefinitions(std::move(spellDefinitions)),
      m_spellCastTables(std::move(spellCastTables)) {}

bool SpellManager::IsSpellKnown(uint32 spellId,
                                std::unordered_set<uint32> const *knownSpells) {
  if (!knownSpells || spellId == 0u)
    return false;
  return knownSpells->find(spellId) != knownSpells->end();
}

void SpellManager::ProcessCastRequest(SpellCastRequest const &req,
                                      SpellCastOutcome *out) const {
  if (!out)
    return;
  out->kind = SpellCastOutcome::Kind::None;
  out->hasDirectHealthEffect = false;
  out->directHealthTargetGuid = 0;
  out->directHealthDelta = 0;
  out->power1Delta = 0;
  out->spellCooldownDurationMs = 0;
  out->spellCategoryCooldownGroup = 0;
  out->spellCategoryCooldownDurationMs = 0;
  out->deferredCastTimeMs = 0;
  out->deferredCastId = 0;
  out->deferredSpellId = 0;
  out->deferredTargetFlags = 0;
  out->deferredTargetUnitGuid = 0;
  out->deferredHitGuid = 0;

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

  std::optional<SpellDefinition> def;
  if (m_spellDefinitions)
    def = m_spellDefinitions->GetDefinition(spellId);

  if (req.now < req.gcdReady) {
    SpellCastWire::BuildSpellFailure(out->failurePacket, req.casterGuid,
                                     req.client.castId, req.client.spellId,
                                     SpellCastWire::SPELL_FAILED_NOT_READY);
    out->kind = SpellCastOutcome::Kind::SpellFailure;
    return;
  }

  uint32 cooldownCategoryRecoveryMs = 0;
  uint32 cooldownRecoveryMs = 0;
  uint32 cooldownGcdMs = 0;
  if (def && m_spellCastTables) {
    m_spellCastTables->GetCooldownTiming(def->cooldownsId, &cooldownCategoryRecoveryMs,
                                         &cooldownRecoveryMs, &cooldownGcdMs);
  }

  if (def && req.spellCooldownUntilBySpellId != nullptr) {
    auto const it = req.spellCooldownUntilBySpellId->find(spellId);
    if (it != req.spellCooldownUntilBySpellId->end() && req.now < it->second) {
      SpellCastWire::BuildSpellFailure(out->failurePacket, req.casterGuid,
                                       req.client.castId, req.client.spellId,
                                       SpellCastWire::SPELL_FAILED_NOT_READY);
      out->kind = SpellCastOutcome::Kind::SpellFailure;
      return;
    }
  }

  uint32 categoryGroup = 0;
  if (def && m_spellCastTables)
    categoryGroup = m_spellCastTables->GetSpellCategoryGroupForCategoriesId(
        def->categoriesId);
  if (def && req.spellCategoryCooldownUntilByGroup != nullptr &&
      cooldownCategoryRecoveryMs > 0u && categoryGroup > 0u) {
    auto const cit =
        req.spellCategoryCooldownUntilByGroup->find(categoryGroup);
    if (cit != req.spellCategoryCooldownUntilByGroup->end() &&
        req.now < cit->second) {
      SpellCastWire::BuildSpellFailure(out->failurePacket, req.casterGuid,
                                       req.client.castId, req.client.spellId,
                                       SpellCastWire::SPELL_FAILED_NOT_READY);
      out->kind = SpellCastOutcome::Kind::SpellFailure;
      return;
    }
  }

  if (def && def->manaCost > 0u && req.hasCasterPowerSnapshot &&
      req.casterPower1 < def->manaCost) {
    SpellCastWire::BuildSpellFailure(out->failurePacket, req.casterGuid,
                                     req.client.castId, req.client.spellId,
                                     SpellCastWire::SPELL_FAILED_NO_POWER);
    out->kind = SpellCastOutcome::Kind::SpellFailure;
    return;
  }

  if (def && m_spellCastTables) {
    bool const friendlyCols = SpellUsesFriendlySpellRangeColumns(&*def, req);
    float const maxYards =
        m_spellCastTables->GetSpellRangeMaxYards(def->rangeIndex, friendlyCols);
    float const minYards =
        m_spellCastTables->GetSpellRangeMinYards(def->rangeIndex, friendlyCols);
    if ((maxYards > 0.f || minYards > 0.f) && req.hasCasterWorldPosition &&
        req.hasTargetWorldPosition) {
      float const dx = req.targetX - req.casterX;
      float const dy = req.targetY - req.casterY;
      float const dz = req.targetZ - req.casterZ;
      float const dist = std::sqrt(dx * dx + dy * dy + dz * dz);
      // TCPP `MAX_SPELL_RANGE_TOLERANCE` (yards) — small slack so borderline casts match client.
      constexpr float kMaxRangeToleranceYards = 3.0f;
      constexpr float kMinRangeToleranceYards = 1.5f;
      if (maxYards > 0.f && dist > maxYards + kMaxRangeToleranceYards) {
        SpellCastWire::BuildSpellFailure(
            out->failurePacket, req.casterGuid, req.client.castId, req.client.spellId,
            SpellCastWire::SPELL_FAILED_OUT_OF_RANGE);
        out->kind = SpellCastOutcome::Kind::SpellFailure;
        return;
      }
      if (minYards > 0.f && dist + kMinRangeToleranceYards < minYards) {
        SpellCastWire::BuildSpellFailure(
            out->failurePacket, req.casterGuid, req.client.castId, req.client.spellId,
            SpellCastWire::SPELL_FAILED_TOO_CLOSE);
        out->kind = SpellCastOutcome::Kind::SpellFailure;
        return;
      }
    }
  }

  bool const ignoreLosFromSpell =
      def && (def->attributesEx2 & SpellAttr2::kIgnoreLineOfSight) != 0u;
  if (!req.skipLineOfSight && !ignoreLosFromSpell && req.collisionQueries != nullptr &&
      req.collisionQueries->IsNavMeshDataAvailable(req.mapId) &&
      req.hasCasterWorldPosition && req.hasTargetWorldPosition &&
      req.client.unitTargetGuid != 0u &&
      req.client.unitTargetGuid != req.casterGuid) {
    if (!req.collisionQueries->LineOfSight(
            req.mapId, req.casterX, req.casterY, req.casterZ, req.targetX,
            req.targetY, req.targetZ)) {
      SpellCastWire::BuildSpellFailure(
          out->failurePacket, req.casterGuid, req.client.castId, req.client.spellId,
          SpellCastWire::SPELL_FAILED_LINE_OF_SIGHT);
      out->kind = SpellCastOutcome::Kind::SpellFailure;
      return;
    }
  }

  uint32 castTimeStart = 0;
  if (def && m_spellCastTables)
    castTimeStart = m_spellCastTables->GetCastTimeMs(def->castingTimeIndex);

  uint32 targetFlags = req.client.targetFlags;
  uint64 targetUnitGuid = req.casterGuid;
  if ((req.client.targetFlags & SpellCastWire::ClientTargetPrimaryGuidMask) == 0) {
    targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
    targetUnitGuid = req.casterGuid;
  } else {
    targetUnitGuid = req.client.unitTargetGuid != 0 ? req.client.unitTargetGuid
                                                    : req.casterGuid;
  }

  uint64 const hitGuid = SpellHitEffects::ResolvePrimarySpellHitUnitGuid(
      req.client.targetFlags, req.casterGuid, req.client.unitTargetGuid);

  SpellHitEffects::ApplyImmediateHealthFromDefinition(
      def.has_value() ? &*def : nullptr, hitGuid, out);

  if (cooldownRecoveryMs > 0u)
    out->spellCooldownDurationMs = cooldownRecoveryMs;
  if (cooldownCategoryRecoveryMs > 0u && categoryGroup > 0u) {
    out->spellCategoryCooldownGroup = categoryGroup;
    out->spellCategoryCooldownDurationMs = cooldownCategoryRecoveryMs;
  }

  uint32 gcdDuration = cooldownGcdMs > 0u ? cooldownGcdMs : 1500u;
  gcdDuration = std::min(gcdDuration, 10000u);
  out->newGcdReady =
      req.now + std::chrono::milliseconds(static_cast<int64_t>(gcdDuration));

  if (def && def->manaCost > 0u && req.hasCasterPowerSnapshot &&
      req.casterPower1 >= def->manaCost)
    out->power1Delta = -static_cast<int32>(def->manaCost);

  uint32 const castFlagsStart = SpellCastWire::CAST_FLAG_HAS_TRAJECTORY;

  SpellCastWire::BuildSpellStart(out->spellStart, req.casterGuid, req.client.castId,
                                 spellId, castFlagsStart, 0, castTimeStart, targetFlags,
                                 targetUnitGuid);

  bool const resolveInstantly = (castTimeStart == 0u);
  if (resolveInstantly) {
    uint32 const castFlagsGo = SpellCastWire::CAST_FLAG_UNKNOWN_9;
    uint32 const castTimeGo = static_cast<uint32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            req.now.time_since_epoch())
            .count());
    uint64 const hitTargets[1] = {hitGuid};
    SpellCastWire::BuildSpellGo(out->spellGo, req.casterGuid, req.client.castId, spellId,
                                castFlagsGo, 0, castTimeGo, hitTargets, 1, targetFlags,
                                targetUnitGuid);
    out->kind = SpellCastOutcome::Kind::SpellStartAndGo;
  } else {
    out->kind = SpellCastOutcome::Kind::SpellStartDeferred;
    out->deferredCastTimeMs = castTimeStart;
    out->deferredCastId = req.client.castId;
    out->deferredSpellId = spellId;
    out->deferredTargetFlags = targetFlags;
    out->deferredTargetUnitGuid = targetUnitGuid;
    out->deferredHitGuid = hitGuid;
  }
}

} // namespace Firelands
