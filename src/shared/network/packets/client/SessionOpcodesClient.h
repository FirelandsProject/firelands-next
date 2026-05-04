#pragma once

#include <cstdint>
#include <shared/network/WorldPacket.h>

namespace Firelands::WorldPackets::Client {

/// `CMSG_PING` — client latency probe; server answers with `SMSG_PONG` same serial.
struct PingRequest {
  uint32_t serial = 0;
  static void Read(WorldPacket &packet, PingRequest &out) {
    out.serial = packet.Read<uint32>();
  }
};

/// `CMSG_TIME_SYNC_RESP` — ack to `SMSG_TIME_SYNC_REQ`.
struct TimeSyncResponse {
  uint32_t counter = 0;
  uint32_t clientTime = 0;
  static void Read(WorldPacket &packet, TimeSyncResponse &out) {
    out.counter = packet.Read<uint32>();
    out.clientTime = packet.Read<uint32>();
  }
};

/// `CMSG_ZONEUPDATE` — new zone id for session bookkeeping.
struct ZoneUpdateRequest {
  uint32_t newZoneId = 0;
  static void Read(WorldPacket &packet, ZoneUpdateRequest &out) {
    out.newZoneId = 0;
    if (packet.Size() - packet.GetReadPos() >= sizeof(uint32))
      out.newZoneId = packet.Read<uint32>();
  }
};

} // namespace Firelands::WorldPackets::Client
