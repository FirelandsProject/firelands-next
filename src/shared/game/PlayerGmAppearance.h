#pragma once

#include <shared/Common.h>
#include <map>

namespace Firelands {

/// Bits aligned with Cataclysm 4.3.4 client expectations (`PlayerFlags`).
inline constexpr uint32 PLAYER_FLAGS_GM_TAG = 0x00000008;
inline constexpr uint32 PLAYER_FLAGS_DND = 0x00000004;
inline constexpr uint32 PLAYER_FLAGS_DEVELOPER = 0x00008000;
inline constexpr uint32 UNIT_FLAG_INVISIBLE = 0x00020000;

/// Staff chat / nameplate flags for `PLAYER_FLAGS` + visibility on `UNIT_FIELD_FLAGS`.
struct PlayerGmAppearanceForUpdates {
  bool gmTagOn = false;
  bool dndOn = false;
  bool devTagOn = false;
  /// When false, other clients receive `UNIT_FLAG_INVISIBLE` (GM hidden).
  bool visibleToOthers = true;
};

void MergeGmAppearanceIntoPlayerFields(std::map<uint16, uint32> &fields,
                                       PlayerGmAppearanceForUpdates const &gm);

/// Live `UNIT_FIELD_FLAGS` for player values updates (swim + GM invis + combat).
uint32 ComputePlayerWireUnitFieldFlags(PlayerGmAppearanceForUpdates const &gm,
                                       bool inCombat);

} // namespace Firelands
