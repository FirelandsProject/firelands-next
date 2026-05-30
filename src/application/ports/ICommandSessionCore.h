#pragma once

#include <shared/game/AccessLevel.h>
#include <shared/Common.h>
#include <cstdint>
#include <string>

namespace Firelands {

struct MovementInfo;
class WorldPacket;

/// Session surface shared by console and world client (non-GM).
class ICommandSessionCore {
public:
  virtual ~ICommandSessionCore() = default;
  virtual void SendNotification(const std::string &message) = 0;
  virtual void SendScreenNotification(std::string const &message) {
    (void)message;
  }
  virtual void SendGmResponseReceived(uint32_t ticketId,
                                      std::string const &playerMessage,
                                      std::string const &gmResponse) {
    (void)ticketId;
    (void)playerMessage;
    (void)gmResponse;
  }
  virtual uint32_t GetAccountId() const { return 0; }
  virtual const MovementInfo &GetPosition() const = 0;
  virtual uint32 GetMapId() const { return 0; }
  virtual void TeleportTo(uint32_t mapId, float x, float y, float z,
                          float orientation = 0.0f) = 0;
  virtual AccessLevel GetAccountAccessLevel() const = 0;
  virtual void RequestDisconnect(std::string const &reason) { (void)reason; }
  virtual void SendPacket(WorldPacket &packet) { (void)packet; }
  virtual uint64_t GetClientSelectionGuid() const { return 0; }
  virtual uint64_t GetActiveCharacterObjectGuid() const { return 0; }
};

} // namespace Firelands
