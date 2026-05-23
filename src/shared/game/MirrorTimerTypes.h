#pragma once

#include <cstdint>

namespace Firelands {

/// Client mirror bar ids (`Player::SendMirrorTimer` / `SMSG_START_MIRROR_TIMER`).
enum class MirrorTimerType : int32_t {
  Fatigue = 0,
  Breath = 1,
  Fire = 2,
};

/// Default underwater breath duration (`Player::getMaxTimer(BREATH_TIMER)`).
inline constexpr int32_t kBreathMirrorMaxMs = 3 * 60 * 1000;

} // namespace Firelands
