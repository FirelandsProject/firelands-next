#pragma once

#include <cstdint>

namespace Firelands {

/// Trinity `Unit::getClassMask()` / `Player::SatisfyQuestClass`.
inline constexpr uint32_t PlayerClassMask(uint8_t classId) noexcept {
  return classId == 0 ? 0u : (1u << (classId - 1u));
}

/// Trinity `Unit::getRaceMask()` / `Player::SatisfyQuestRace`.
inline constexpr uint32_t PlayerRaceMask(uint8_t raceId) noexcept {
  return raceId == 0 ? 0u : (1u << (raceId - 1u));
}

/// `AllowableClasses` / `AllowableRaces` from `quest_template_addon` (0 = no restriction).
inline constexpr bool QuestMaskAllowsPlayer(uint32_t allowableMask,
                                            uint32_t playerMask) noexcept {
  return allowableMask == 0 || (allowableMask & playerMask) != 0;
}

} // namespace Firelands
