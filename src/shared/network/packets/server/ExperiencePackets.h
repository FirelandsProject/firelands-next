#pragma once

#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

namespace Firelands::experience_wire {

enum class LogXpReason : uint8_t {
  Kill = 0,
  NoKill = 1,
};

/// Cataclysm 4.3.4 — `WorldPackets::Character::LogXPGain::Write` (on-screen XP floater + combat log).
inline WorldPacket BuildLogXpGain(uint64_t victimGuid, int32_t totalExp, int32_t amount,
                                  float groupBonus = 1.0f,
                                  LogXpReason reason = LogXpReason::Kill,
                                  uint8_t referAFriendBonusType = 0) {
  WorldPacket pkt(SMSG_LOG_XP_GAIN, 32);
  pkt.Append<uint64>(victimGuid);
  pkt.Append<int32_t>(totalExp);
  pkt.Append<uint8_t>(static_cast<uint8_t>(reason));
  if (reason == LogXpReason::Kill) {
    pkt.Append<int32_t>(amount);
    pkt.Append<float>(groupBonus);
  }
  pkt.Append<uint8_t>(referAFriendBonusType);
  return pkt;
}

} // namespace Firelands::experience_wire
