#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/Common.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

class IMapCollisionQueries;

/// Input for a client-initiated cast after `TryReadClientCastSpell` succeeds.
/// `knownSpells` must outlive the call (typically `WorldSession::_knownSpells`).
struct SpellCastRequest {
  uint64 casterGuid = 0;
  uint32 mapId = 0;
  SpellCastWire::ClientCastSpellData client{};
  std::chrono::steady_clock::time_point now{};
  std::chrono::steady_clock::time_point gcdReady{};
  std::vector<uint32> const *knownSpells = nullptr;
  /// When both are set, `SpellManager` checks 3D distance vs `SpellRange` hostile max
  /// (plus TCPP-style tolerance). Otherwise range is skipped until the map provides
  /// target positions for non-self casts.
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
};

/// Result of `SpellManager::ProcessCastRequest`: packets to send and new GCD time.
/// Caller performs `SendPacket` / map broadcast. Hot path: avoid heap allocs here.
struct SpellCastOutcome {
  enum class Kind : uint8 { None, SpellFailure, SpellStartAndGo };
  Kind kind = Kind::None;
  WorldPacket failurePacket;
  WorldPacket spellStart;
  WorldPacket spellGo;
  std::chrono::steady_clock::time_point newGcdReady{};
  /// Phase D: single direct health change on spell hit (see `SpellDefinition`).
  bool hasDirectHealthEffect = false;
  uint64 directHealthTargetGuid = 0;
  int32 directHealthDelta = 0;
};

/// Centralizes spell cast validation and server-side spell wire output (Phase A+).
/// Phase D MVP effects (e.g. direct health) are encoded in `SpellCastOutcome`; a future
/// `IEffectHandler` chain can split this without changing the wire contract.
/// Thread-safety: const methods only; per-session mutable state stays on `WorldSession`
/// (`_gcdReady`, `_knownSpells`) passed in/out via `SpellCastRequest` / `SpellCastOutcome`.
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
  static bool IsSpellKnown(uint32 spellId, std::vector<uint32> const *knownSpells);

  std::shared_ptr<ISpellDefinitionStore const> m_spellDefinitions;
  std::shared_ptr<ISpellCastTables const> m_spellCastTables;
};

} // namespace Firelands
