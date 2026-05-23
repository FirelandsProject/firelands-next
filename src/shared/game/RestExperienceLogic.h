#pragma once

#include <cstdint>

namespace Firelands::RestExperienceLogic {

/// Cataclysm 4.3.4 `PLAYER_BYTES_2` byte index (`Player.h`).
inline constexpr uint8_t kPlayerBytes2OffsetRestState = 3u;

/// `Player::REST_STATE_*` wire values for `PLAYER_BYTES_2` rest-state byte.
enum class RestStateWire : uint8_t {
  Rested = 1,
  NotRafLinked = 2,
};

struct RestConsumeResult {
  uint32_t restedBonus = 0;
  float restBonusAfter = 0.f;
};

float RestBonusCap(uint32_t nextLevelXp);
float ClampRestBonus(float restBonus, uint32_t nextLevelXp);
RestConsumeResult ConsumeForKill(float restBonus, uint32_t baseXp, uint32_t nextLevelXp);
RestStateWire RestStateForBonus(float restBonus);
uint32_t RestBonusWireAmount(float restBonus);

} // namespace Firelands::RestExperienceLogic
