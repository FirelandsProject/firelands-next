#pragma once

#include <shared/Common.h>
#include <shared/game/PlayerPowerType.h>
#include <chrono>
#include <cstdint>

namespace Firelands {

struct ResourceRegenDelta {
  int32 health = 0;
  int32 power1 = 0;
};

/// Bonuses from racials/passives (`MOD_HEALTH_REGEN_PERCENT`, `MOD_REGEN_DURING_COMBAT`, …).
struct ResourceRegenModifiers {
  /// Additive percent on health regen rate (Troll Regeneration +10).
  int32 healthRegenPct = 0;
  /// Percent of normal OOC health regen allowed while in combat (Troll second effect +10).
  int32 healthRegenDuringCombatPct = 0;
  /// Additive percent on POWER1 regen (mana/energy where applicable).
  int32 powerRegenPct = 0;
};

/// Reference-style passive regen for Cataclysm POWER1 types (interval-scaled).
ResourceRegenDelta ComputeResourceRegenDelta(PlayerPowerType powerType, uint8 level,
                                             uint32 spirit, uint32 currentHealth,
                                             uint32 maxHealth, uint32 currentPower1,
                                             uint32 maxPower1, bool outOfCombatForRegen,
                                             std::chrono::milliseconds interval,
                                             ResourceRegenModifiers const &modifiers = {});

} // namespace Firelands
