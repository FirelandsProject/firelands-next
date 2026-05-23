#pragma once

#include <shared/network/BitWriter.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

namespace Firelands::combat_wire {

/// Cataclysm 4.3.4.15595 — `SMSG_ATTACK_START` (attacker then victim).
inline WorldPacket BuildAttackStart(uint64_t attackerGuid, uint64_t victimGuid) {
  WorldPacket pkt(SMSG_ATTACK_START, 32);
  pkt.WritePackedGuid(attackerGuid);
  pkt.WritePackedGuid(victimGuid);
  return pkt;
}

inline WorldPacket BuildAttackSwingDeadTarget() {
  return WorldPacket(SMSG_ATTACKSWING_DEADTARGET, 4);
}

inline WorldPacket BuildAttackSwingCantAttack() {
  return WorldPacket(SMSG_ATTACKSWING_CANT_ATTACK, 4);
}

inline WorldPacket BuildAttackSwingNotInRange() {
  return WorldPacket(SMSG_ATTACKSWING_NOTINRANGE, 4);
}

/// `SMSG_ATTACK_STOP` — packed guids + 1-bit NowDead (not a uint32).
inline WorldPacket BuildAttackStop(uint64_t attackerGuid, uint64_t victimGuid, bool victimNowDead) {
  WorldPacket pkt(SMSG_ATTACKSTOP, 32);
  pkt.WritePackedGuid(attackerGuid);
  pkt.WritePackedGuid(victimGuid);
  BitWriter bw(pkt);
  bw.WriteBit(victimNowDead);
  bw.Flush();
  return pkt;
}

/// `SMSG_ATTACKERSTATE_UPDATE` — ref `WorldPackets::CombatLog::AttackerStateUpdate::Write`.
inline WorldPacket BuildAttackerStateUpdate(uint64_t attackerGuid, uint64_t victimGuid,
                                            uint32_t damage, uint32_t victimHealthAfter) {
  constexpr int32_t kHitInfoNormal = 0x00000001;
  constexpr uint8_t kVictimStateHit = 1u;

  int32_t overDamage = -1;
  if (damage > victimHealthAfter)
    overDamage = static_cast<int32_t>(damage - victimHealthAfter);

  WorldPacket pkt(SMSG_ATTACKERSTATE_UPDATE, 64);
  pkt.Append<int32_t>(kHitInfoNormal);
  pkt.WritePackedGuid(attackerGuid);
  pkt.WritePackedGuid(victimGuid);
  pkt.Append<int32_t>(static_cast<int32_t>(damage));
  pkt.Append<int32_t>(overDamage);
  pkt.Append<uint8_t>(1u);
  pkt.Append<int32_t>(1);
  pkt.Append<float>(static_cast<float>(damage));
  pkt.Append<int32_t>(static_cast<int32_t>(damage));
  pkt.Append<uint8_t>(kVictimStateHit);
  pkt.Append<uint32_t>(0u);
  pkt.Append<uint32_t>(0u);
  return pkt;
}

/// `SMSG_POWER_UPDATE` — Cataclysm 4.3.4 (`WorldPackets::Combat::PowerUpdate::Write` in cata-ref).
inline WorldPacket BuildPowerUpdate(uint64_t unitGuid, uint8_t powerType, int32_t amount) {
  WorldPacket pkt(SMSG_POWER_UPDATE, 32);
  pkt.WritePackedGuid(unitGuid);
  pkt.Append<uint32_t>(1u);
  pkt.Append<uint8_t>(powerType);
  pkt.Append<int32_t>(amount);
  return pkt;
}

/// `SMSG_SPELLNONMELEEDAMAGELOG` — ref `Unit::SendSpellNonMeleeDamageLog` (target guid first).
inline WorldPacket BuildSpellNonMeleeDamageLog(uint64_t targetGuid, uint64_t attackerGuid,
                                               uint32_t spellId, uint32_t damage,
                                               uint32_t targetHealthAfter,
                                               uint8_t schoolMask = 1) {
  uint32_t const overkill =
      damage > targetHealthAfter ? damage - targetHealthAfter : 0u;

  WorldPacket pkt(SMSG_SPELLNONMELEEDAMAGELOG, 64);
  pkt.WritePackedGuid(targetGuid);
  pkt.WritePackedGuid(attackerGuid);
  pkt.Append<uint32_t>(spellId);
  pkt.Append<uint32_t>(damage);
  pkt.Append<uint32_t>(overkill);
  pkt.Append<uint8_t>(schoolMask);
  pkt.Append<uint32_t>(0u);
  pkt.Append<uint32_t>(0u);
  pkt.Append<uint8_t>(0u);
  pkt.Append<uint8_t>(0u);
  pkt.Append<uint32_t>(0u);
  pkt.Append<uint32_t>(0u);
  pkt.Append<uint8_t>(0u);
  return pkt;
}

} // namespace Firelands::combat_wire
