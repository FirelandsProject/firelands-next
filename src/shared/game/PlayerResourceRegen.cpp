#include <shared/game/PlayerResourceRegen.h>

#include <algorithm>
#include <cmath>

namespace Firelands {

namespace {

int32 ScalePerInterval(int32 amountPerTwoSeconds,
                       std::chrono::milliseconds interval) {
  if (amountPerTwoSeconds == 0 || interval.count() <= 0)
    return 0;
  return static_cast<int32>(static_cast<int64>(amountPerTwoSeconds) *
                            interval.count() / 2000);
}

/// Per `UNIT_HEALTH_REGENERATION_INTERVAL` (2s), `Player::RegenerateHealth` in reference.
int32 HealthRegenPerTwoSeconds(uint32 maxHealth, uint8 level) {
  if (maxHealth == 0 || level == 0)
    return 0;
  float addValue = 0.f;
  if (level < 15)
    addValue = 0.20f * static_cast<float>(maxHealth) / static_cast<float>(level);
  else
    addValue = 0.015f * static_cast<float>(maxHealth);
  return static_cast<int32>(std::max(1.f, std::floor(addValue)));
}

} // namespace

ResourceRegenDelta ComputeResourceRegenDelta(PlayerPowerType powerType, uint8 level,
                                             uint32 spirit, uint32 currentHealth,
                                             uint32 maxHealth, uint32 currentPower1,
                                             uint32 maxPower1, bool outOfCombatForRegen,
                                             std::chrono::milliseconds interval,
                                             ResourceRegenModifiers const &modifiers) {
  ResourceRegenDelta out{};

  if (currentHealth < maxHealth && maxHealth > 0) {
    int32 healthPerTwoSec = HealthRegenPerTwoSeconds(maxHealth, level);
    if (modifiers.healthRegenPct != 0) {
      healthPerTwoSec += static_cast<int32>(
          static_cast<int64>(healthPerTwoSec) * modifiers.healthRegenPct / 100);
    }
    bool applyHealthRegen = outOfCombatForRegen;
    if (!applyHealthRegen && modifiers.healthRegenDuringCombatPct > 0) {
      healthPerTwoSec = static_cast<int32>(static_cast<int64>(healthPerTwoSec) *
                                         modifiers.healthRegenDuringCombatPct / 100);
      applyHealthRegen = healthPerTwoSec > 0;
    }
    if (applyHealthRegen)
      out.health = ScalePerInterval(healthPerTwoSec, interval);
  }

  switch (powerType) {
  case PlayerPowerType::Energy:
    if (currentPower1 < maxPower1) {
      int32 perTwo = 20;
      if (modifiers.powerRegenPct != 0)
        perTwo += static_cast<int32>(static_cast<int64>(perTwo) * modifiers.powerRegenPct / 100);
      out.power1 = ScalePerInterval(perTwo, interval);
    }
    break;
  case PlayerPowerType::Focus:
    if (currentPower1 < maxPower1) {
      int32 perTwo = 8;
      if (modifiers.powerRegenPct != 0)
        perTwo += static_cast<int32>(static_cast<int64>(perTwo) * modifiers.powerRegenPct / 100);
      out.power1 = ScalePerInterval(perTwo, interval);
    }
    break;
  case PlayerPowerType::Rage:
    if (outOfCombatForRegen && currentPower1 > 0)
      out.power1 = -ScalePerInterval(50, interval);
    break;
  case PlayerPowerType::RunicPower:
    if (outOfCombatForRegen && currentPower1 > 0)
      out.power1 = -ScalePerInterval(100, interval);
    break;
  case PlayerPowerType::Mana:
  default:
    if (currentPower1 < maxPower1 && maxPower1 > 0) {
      float mp5 = outOfCombatForRegen ? (5.f + static_cast<float>(spirit) * 0.11f)
                                      : static_cast<float>(spirit) * 0.033f;
      if (modifiers.powerRegenPct != 0)
        mp5 *= 1.f + static_cast<float>(modifiers.powerRegenPct) / 100.f;
      float const gain = mp5 * static_cast<float>(interval.count()) / 5000.f;
      out.power1 = static_cast<int32>(std::max(1.f, std::floor(gain)));
    }
    break;
  }

  return out;
}

} // namespace Firelands
