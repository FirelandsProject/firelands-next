#pragma once

#include <cstdint>

namespace Firelands {

/// `UNIT_NPC_FLAGS` bits (`NPCFlags` / `creature_template.npcflag`).
inline constexpr uint32_t kUnitNpcFlagGossip = 0x00000001u;
inline constexpr uint32_t kUnitNpcFlagQuestGiver = 0x00000002u;
inline constexpr uint32_t kUnitNpcFlagFlightMaster = 0x00002000u;

} // namespace Firelands
