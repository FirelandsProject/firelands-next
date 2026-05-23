#pragma once

#include <cstdint>

namespace Firelands {

/// 4.3.4 `QuestGiverStatus` bit flags (`QuestDef.h` in firelands-cata-ref).
/// Sent as `uint32` in `SMSG_QUESTGIVER_STATUS` / `_MULTIPLE`.
enum class QuestGiverDialogStatus : uint32_t {
  None = 0x000,
  Unavailable = 0x002,
  LowLevelAvailable = 0x004,
  LowLevelRewardRep = 0x008,
  LowLevelAvailableRep = 0x010,
  Incomplete = 0x020,
  RewardRep = 0x040,
  AvailableRep = 0x080,
  Available = 0x100,
  Reward2 = 0x200,
  Reward = 0x400,
};

} // namespace Firelands
