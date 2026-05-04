#pragma once

#include <shared/network/ServerPacket.h>
#include <shared/network/WorldOpcodes.h>

namespace Firelands::WorldPackets::Misc {

/// `SMSG_PONG` — answers `CMSG_PING` with the same serial.
class Pong : public ServerPacket {
public:
  explicit Pong(uint32_t serial) : ServerPacket(SMSG_PONG, 8), _serial(serial) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint32>(_serial);
    return &_worldPacket;
  }

private:
  uint32_t _serial;
};

} // namespace Firelands::WorldPackets::Misc
