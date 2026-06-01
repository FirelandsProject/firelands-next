#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <domain/models/SpellDefinition.h>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/Common.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

class IMapCollisionQueries;

/// Input for a client-initiated cast after `TryReadClientCastSpell` succeeds.
/// `knownSpells` must outlive the call (typically `WorldSession::_knownSpellIds`).
struct SpellCastRequest {
  uint64 casterGuid = 0;
  uint32 mapId = 0;
  SpellCastWire::ClientCastSpellData client{};
  std::chrono::steady_clock::time_point now{};
  std::chrono::steady_clock::time_point gcdReady{};
  /// Client movement `time` field (ms) for `SMSG_SPELL_GO`; 0 lets `SpellManager` use fallback.
  uint32 clientTimestampMs = 0;
  std::unordered_set<uint32> const *knownSpells = nullptr;
  /// When both are set, `SpellManager` checks 3D distance vs `SpellRange.dbc` min/max for the
  /// hostile or friendly column pair (plus slack). Otherwise range checks are skipped.
  bool hasCasterWorldPosition = false;
  float casterX = 0.f;
  float casterY = 0.f;
  float casterZ = 0.f;
  bool hasTargetWorldPosition = false;
  float targetX = 0.f;
  float targetY = 0.f;
  float targetZ = 0.f;
  /// When non-null and `!skipLineOfSight`, used after range checks for hostile unit targets.
  /// Callers must keep the underlying service alive for the duration of `ProcessCastRequest`.
  IMapCollisionQueries const *collisionQueries = nullptr;
  /// Bypass LoS (unit tests, GM, or while tuning). Does not affect range checks.
  bool skipLineOfSight = false;
  /// Per-caster spell cooldown end times (`spellId` → instant). Null = skip check.
  std::unordered_map<uint32, std::chrono::steady_clock::time_point> const *
      spellCooldownUntilBySpellId = nullptr;
  /// Per-caster category cooldown (`SpellCategories.Category` group → instant). Null = skip.
  std::unordered_map<uint32, std::chrono::steady_clock::time_point> const *
      spellCategoryCooldownUntilByGroup = nullptr;
  /// When set with `manaCost` on the definition, `SpellManager` validates POWER1.
  bool hasCasterPowerSnapshot = false;
  /// `UnitBytes0` power type (see `PlayerPowerType`); must match `SpellDefinition::powerType`.
  uint8 casterPrimaryPowerType = 0;
  uint32 casterPower1 = 0;
  uint32 casterMaxPower1 = 0;
  /// Base POWER1 for `%` costs (`GetCreateMana`; seeded at login from template max).
  uint32 casterBasePower1 = 0;
  /// Player-vs-player team hint (Alliance/Horde by race). When false, beneficial spells on other
  /// units keep optimistic friendly `SpellRange.dbc` columns (legacy behaviour).
  bool hasTargetFactionReactionHint = false;
  /// Meaningful only if `hasTargetFactionReactionHint`; same faction team ⇒ friendly band.
  bool targetIsFriendlyTeamForSpellRange = false;
  /// Caster level for aura packets (defaults to 1 when unset).
  uint8 casterLevel = 1;
  /// `UNIT_MOD_CAST_HASTE` / Berserking-style bonus (1.0 = none).
  float casterCastHasteMultiplier = 1.f;
};

/// Result of `SpellManager::ProcessCastRequest`: packets to send and new GCD time.
/// Caller performs `SendPacket` / map broadcast. Hot path: avoid heap allocs here.
struct SpellCastOutcome {
  /// `SpellStartDeferred`: only `spellStart` is sent initially; `SMSG_SPELL_GO` and combat
  /// effects run after `deferredCastTimeMs` (session-scheduled). Matches client cast-bar timing.
  enum class Kind : uint8 { None, SpellFailure, SpellStartAndGo, SpellStartDeferred };
  Kind kind = Kind::None;
  WorldPacket failurePacket;
  WorldPacket spellStart;
  WorldPacket spellGo;
  std::chrono::steady_clock::time_point newGcdReady{};
  /// Phase D: single direct health change on spell hit (see `SpellDefinition`).
  bool hasDirectHealthEffect = false;
  uint64 directHealthTargetGuid = 0;
  int32 directHealthDelta = 0;
  uint32 directHealthSchoolMask = 0;
  /// Negative deducts primary resource (POWER1) on the caster when the cast succeeds.
  int32 power1Delta = 0;
  /// If non-zero, caller should set `spellCooldownUntil[spellId] = now + duration`.
  uint32 spellCooldownDurationMs = 0;
  /// If non-zero with `spellCategoryCooldownDurationMs`, caller updates category CD map.
  uint32 spellCategoryCooldownGroup = 0;
  uint32 spellCategoryCooldownDurationMs = 0;
  /// When `kind == SpellStartDeferred`, session waits `deferredCastTimeMs` then sends GO + applies
  /// health/power/cooldown fields below.
  uint32 deferredCastTimeMs = 0;
  uint8 deferredCastId = 0;
  uint32 deferredSpellId = 0;
  uint32 deferredTargetFlags = 0;
  uint64 deferredTargetUnitGuid = 0;
  uint64 deferredHitGuid = 0;
  bool deferredSpellGoMissile = false;
  SpellCastWire::SpellMissileTrajectoryWire deferredMissile{};
  /// Primary hit unit for `SMSG_SPELL_GO` (impact VFX, combat effects).
  uint64 primaryHitTargetGuid = 0;
  /// Delay before `SMSG_PLAY_SPELL_VISUAL_KIT` when GO carries missile travel time.
  uint32 spellImpactDelayMs = 0;
  /// Phase F: aura apply on successful hit (see `SpellHitEffects::ApplyAuraFromDefinition`).
  bool hasAuraApply = false;
  uint64 auraTargetGuid = 0;
  uint64 auraCasterGuid = 0;
  uint32 auraSpellId = 0;
  uint32 auraEffectType = 0;
  uint8 auraEffectIndex = 0;
  int32 auraBasePoints = 0;
  int32 auraDieSides = 0;
  uint32 auraDurationMs = 0;
  uint32 auraPeriodicPeriodMs = 0;
  int32 auraPeriodicHealthDeltaPerTick = 0;
  bool auraIsNegative = false;
  uint8 auraCasterLevel = 1;
};

/// Centralizes spell cast validation and server-side spell wire output (Phase A+).
/// Phase D hit payloads (direct health, …) are composed via `SpellHitEffects::*` into
/// `SpellCastOutcome`; add functions there to extend behavior without bloating this class.
/// Thread-safety: const methods only; per-session mutable state stays on `WorldSession`
/// (`_gcdReady`, `_knownSpellIds`) passed in/out via `SpellCastRequest` / `SpellCastOutcome`.
class SpellManager {
public:
  explicit SpellManager(
      std::shared_ptr<ISpellDefinitionStore const> spellDefinitions = nullptr,
      std::shared_ptr<ISpellCastTables const> spellCastTables = nullptr);

  /// Evaluates a cast request. On success, sets `newGcdReady` and fills start/go packets.
  /// On failure, fills `failurePacket` only. Does not send on the wire.
  void ProcessCastRequest(SpellCastRequest const &req,
                          SpellCastOutcome *out) const;

private:
  static bool IsSpellKnown(uint32 spellId,
                           std::unordered_set<uint32> const *knownSpells);

  std::shared_ptr<ISpellDefinitionStore const> m_spellDefinitions;
  std::shared_ptr<ISpellCastTables const> m_spellCastTables;
};

} // namespace Firelands
