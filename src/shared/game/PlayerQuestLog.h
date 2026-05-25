#pragma once

#include <map>
#include <shared/network/UpdateFields.h>
#include <cstdint>

namespace Firelands {

inline constexpr uint8_t kMaxQuestLogSlots = 25u;
inline constexpr uint8_t kQuestLogFieldsPerSlot = 5u;

/// `Player::QUEST_STATE_*` on `PLAYER_QUEST_LOG_*_2`.
inline constexpr uint32_t kPlayerQuestLogStateComplete = 0x0001u;

/// First update-field index for quest log slot `slot` (0..24).
inline constexpr uint16_t PlayerQuestLogFieldBase(uint8_t slot) {
  return static_cast<uint16>(PLAYER_QUEST_LOG_1_1 + slot * kQuestLogFieldsPerSlot);
}

/// Mirrors ref `Player::SetQuestSlot` (`MAX_QUEST_OFFSET` = 5).
inline void WriteQuestLogSlotFields(std::map<uint16, uint32> &fields, uint8_t slot,
                                    uint32_t questId, uint32_t questStateFlags = 0u,
                                    uint32_t timerMs = 0u) {
  if (slot >= kMaxQuestLogSlots)
    return;
  uint16 const base = PlayerQuestLogFieldBase(slot);
  fields[base] = questId;
  fields[static_cast<uint16>(base + 1)] = questStateFlags;
  fields[static_cast<uint16>(base + 2)] = 0u;
  fields[static_cast<uint16>(base + 3)] = 0u;
  fields[static_cast<uint16>(base + 4)] = timerMs;
}

} // namespace Firelands
