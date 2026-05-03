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
