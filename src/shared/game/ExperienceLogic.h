#pragma once

#include <shared/game/ExperienceRates.h>
#include <cstdint>
#include <functional>

namespace Firelands {

namespace ExperienceLogic {

constexpr uint8_t kMaxPlayerLevelCata = 85;

/// Gray level bands: creatures at or below this level grant no kill XP.
uint8_t GrayLevelForPlayer(uint8_t playerLevel);

/// Base kill XP before level-diff and rate modifiers (Cataclysm band approximation).
uint32_t BaseKillExperienceForCreatureLevel(uint8_t creatureLevel);

/// Kill XP for a player killing a creature (0 when gray or at max level).
uint32_t CalculateKillExperience(uint8_t playerLevel, uint8_t creatureLevel,
                                 float creatureExperienceModifier,
                                 ExperienceRates const &rates);

struct ExperienceGainResult {
  uint8_t level = 1;
  uint32_t xp = 0;
  uint8_t levelsGained = 0;
};

/// Applies `amount` XP, leveling up while `xpToNext(currentLevel)` is exceeded.
ExperienceGainResult ApplyExperienceGain(
    uint8_t level, uint32_t xp, uint32_t amount, uint8_t maxLevel,
    std::function<uint32_t(uint8_t)> const &xpToNext);

} // namespace ExperienceLogic

} // namespace Firelands
