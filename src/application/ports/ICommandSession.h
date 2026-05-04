#pragma once

#include <shared/game/AccessLevel.h>
#include <shared/Common.h>
#include <cstdint>
#include <string>

namespace Firelands {

struct MovementInfo;

/// Minimal surface used by `ICommandService` / `CommandService` (in-game client or
/// server REPL).
class ICommandSession {
public:
  virtual ~ICommandSession() = default;
  virtual void SendNotification(const std::string &message) = 0;
  /// Center-screen banner (`SMSG_NOTIFICATION`); no-op for console / stubs.
  virtual void SendScreenNotification(std::string const &message) {
    (void)message;
  }
  /// `SMSG_GMRESPONSE_RECEIVED` (in-world only; default no-op).
  virtual void SendGmResponseReceived(uint32_t ticketId,
                                      std::string const &playerMessage,
                                      std::string const &gmResponse) {
    (void)ticketId;
    (void)playerMessage;
    (void)gmResponse;
  }

  /// Auth `account.id` for the connected world client; 0 for console stub.
  virtual uint32_t GetAccountId() const { return 0; }
  virtual const MovementInfo &GetPosition() const = 0;
  virtual uint32 GetMapId() const { return 0; }
  virtual void TeleportTo(uint32_t mapId, float x, float y, float z,
                          float orientation = 0.0f) = 0;
  virtual AccessLevel GetAccountAccessLevel() const = 0;

  /// Graceful disconnect (e.g. `.kick`); no-op for console stub.
  virtual void RequestDisconnect(std::string const &reason) { (void)reason; }

  /// Gameplay GM helpers (no-op / false unless `WorldSession`).
  virtual bool GmLearnSpell(uint32 spellId) {
    (void)spellId;
    return false;
  }
  virtual bool GmModifyMoneyCopper(int64 deltaCopper) {
    (void)deltaCopper;
    return false;
  }
  virtual bool GmAddItem(uint32 itemEntry, uint32 count) {
    (void)itemEntry;
    (void)count;
    return false;
  }
  /// Removes up to `count` items matching `itemEntry` from the main backpack (bag 0).
  virtual bool GmRemoveItem(uint32 itemEntry, uint32 count) {
    (void)itemEntry;
    (void)count;
    return false;
  }
  virtual bool GmSetLevel(uint8 level) {
    (void)level;
    return false;
  }

  /// Spawns a creature at this session's map position and facing (world server).
  /// \param factionTemplateOrZeroDefault `FactionTemplate.dbc` id. `0` uses
  /// `creature_template.faction` for `creatureEntry` when the world DB has that column/row;
  /// if still unknown, falls back to `Creature::kDefaultFactionTemplate`.
  virtual bool GmSpawnNpc(uint32 creatureEntry, uint32 displayId,
                          uint32 factionTemplateOrZeroDefault = 0) {
    (void)creatureEntry;
    (void)displayId;
    (void)factionTemplateOrZeroDefault;
    return false;
  }

  /// Despawns a creature on this session's current map (`TryGetCreature`).
  virtual bool GmDeleteNpcByObjectGuid(uint64 objectGuid) {
    (void)objectGuid;
    return false;
  }

  /// `SMSG_SET_FORCED_REACTIONS`: force how the client treats `factionDbcId` (0–7 = `ReputationRank`).
  virtual bool GmSetForcedFactionReaction(uint32 factionDbcId, uint8 reputationRank) {
    (void)factionDbcId;
    (void)reputationRank;
    return false;
  }
  virtual bool GmClearForcedFactionReaction(uint32 factionDbcId) {
    (void)factionDbcId;
    return false;
  }
  virtual bool GmClearAllForcedFactionReactions() { return false; }
  /// `UNIT_FIELD_FACTIONTEMPLATE` for this session's player (broadcast to self + observers).
  virtual bool GmSetOwnFactionTemplate(uint32 factionTemplate) {
    (void)factionTemplate;
    return false;
  }
  /// Same as `GmSetOwnFactionTemplate` but for `CMSG_SET_SELECTION` creature on this map.
  virtual bool GmSetSelectedCreatureFactionTemplate(uint32 factionTemplate) {
    (void)factionTemplate;
    return false;
  }

  /// GM NPC template search: prints styled matches to system chat (no gossip UI).
  virtual bool GmNpcSearchPrintResults(std::string const &nameQuery) {
    (void)nameQuery;
    return false;
  }

  /// Client `CMSG_SET_SELECTION` target (0 = none). Used by GM item commands in-game.
  virtual uint64_t GetClientSelectionGuid() const { return 0; }
  /// World `ObjectGuid` for the logged-in character (0 when not in world / console).
  virtual uint64_t GetActiveCharacterObjectGuid() const { return 0; }

  /// Optional GM tooling (no-op for console stub / non-world sessions).
  virtual void SetGmTagEnabled(bool on) { (void)on; }
  virtual void SetDndEnabled(bool on) { (void)on; }
  virtual void SetDevTagEnabled(bool on) { (void)on; }
  virtual void SetGmVisibleToPlayers(bool visible) { (void)visible; }
  virtual void SetGmFlyEnabled(bool on) { (void)on; }
  virtual void SetGmRunSpeed(float speed) { (void)speed; }

  /// Opens the mailbox UI (`SMSG_SHOW_MAILBOX`); default no-op (world client only).
  virtual void OpenGmMailboxUi() {}
};

} // namespace Firelands
