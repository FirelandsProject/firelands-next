#pragma once

#include <application/spell/SpellManager.h>
#include <domain/world/Aura.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Firelands {

class Map;

/// Applies aura from `SpellCastOutcome` (server state + `SMSG_AURA_UPDATE`). Call after
/// `SMSG_SPELL_GO` so the client uses the server visual slot (avoids GO-only ghost buffs).
void ApplySpellCastAuraOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                             SpellCastOutcome const &outcome,
                             std::chrono::steady_clock::time_point now);

/// Re-sends active auras for `unitGuid` (login / reconnect).
void SendActiveAurasOnMap(std::shared_ptr<Map> const &map, uint64 unitGuid,
                             std::chrono::steady_clock::time_point now);

/// Recomputes aura-derived stat/rating fields and broadcasts `SMSG_UPDATE_OBJECT`.
void BroadcastPlayerAuraStatBonusOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                       uint64 unitGuid, uint8 casterLevel);

/// Applies passive auras for `candidateSpellIds` (login: all known passive combat spells).
void ApplyPassiveAurasForKnownSpellsOnMap(
    uint32 mapId, std::shared_ptr<Map> const &map, uint64_t unitGuid,
    uint8_t casterLevel, std::vector<uint32_t> const &candidateSpellIds,
                             std::chrono::steady_clock::time_point now);

/// Set when a player caster reduces a creature to 0 HP (for kill XP).
struct CreatureKillByPlayerHint {
  uint64 creatureGuid = 0;
  uint32 hpBefore = 0;
};

/// Applies POWER1 delta on the map player (no wire).
bool ApplyPlayerPower1DeltaOnMap(std::shared_ptr<Map> const &map, uint64 casterGuid,
                                                                  int32 power1Delta);

/// `SMSG_POWER_UPDATE` + values update. Send to caster session first, then nearby (before spell GO).
void BroadcastPlayerPower1OnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                uint64 playerGuid, uint8 primaryPowerType);

                                                                /// Deducts spell power on the map. Notify with `BroadcastPlayerPower1OnMap` immediately before spell GO.
                                                                bool ApplyPlayerSpellPowerCostOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                                                                        uint64 casterGuid, int32 power1Delta);

                                                                        /// Applies direct health from `SpellCastOutcome` on `map` and broadcasts wire.
                                                                        /// Power is applied via `ApplyPlayerSpellPowerCostOnMap`; notify with `BroadcastPlayerPower1OnMap`.
std::optional<CreatureKillByPlayerHint> ApplySpellCastOutcomeOnMap(
    uint32 mapId, std::shared_ptr<Map> const &map, uint64 casterGuid, uint32 spellId,
    SpellCastOutcome const &outcome, std::chrono::steady_clock::time_point now);

/// Broadcasts target impact VFX (`SMSG_PLAY_SPELL_VISUAL_KIT`) for a successful spell hit.
void BroadcastSpellImpactVisualOnMap(std::shared_ptr<Map> const &map, uint64 nearbyAnchorGuid,
                                     uint32 spellId, uint64 hitTargetGuid);

/// Sends `SMSG_AURA_UPDATE` remove (slot + spell id 0) and refreshes player aura stat fields.
void SendAuraRemovalOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                          uint64 unitGuid, AuraRemoval const &removal);

/// Sends HoT tick log, optional aura remaining refresh, and health values update.
void SendPeriodicHealTickOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                               uint64 unitGuid, AuraPeriodicTick const &tick);

/// Removes an active aura on `unitGuid` and sends remove wire. @return false if not found.
bool RemovePlayerAuraOnMap(uint32 mapId, std::shared_ptr<Map> const &map,
                           uint64 unitGuid, uint32 spellId, uint8 casterLevel);

/// Removes `spellId` on any unit; when `casterGuid` is set, only that caster's aura.
bool RemoveAuraOnMapBySpellId(uint32 mapId, std::shared_ptr<Map> const &map,
                              uint32 spellId, uint64 casterGuid = 0);

/// Broadcasts `UNIT_FIELD_HEALTH` / max health after a combat or spell delta.
void BroadcastUnitHealthAfterDelta(uint32 mapId, std::shared_ptr<Map> const &map,
                                   uint64 unitGuid, uint32 health, uint32 maxHealth);

} // namespace Firelands
