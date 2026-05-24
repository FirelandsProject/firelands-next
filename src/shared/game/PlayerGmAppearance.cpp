#include <shared/game/PlayerGmAppearance.h>
#include <shared/game/UnitFieldFlags.h>
#include <shared/network/UpdateFields.h>

namespace Firelands {

void MergeGmAppearanceIntoPlayerFields(std::map<uint16, uint32> &fields,
                                       PlayerGmAppearanceForUpdates const &gm) {
  uint32 pf = 0;
  if (auto it = fields.find(static_cast<uint16>(PLAYER_FLAGS));
      it != fields.end()) {
    pf = it->second;
  }
  if (gm.gmTagOn)
    pf |= PLAYER_FLAGS_GM_TAG;
  else
    pf &= ~PLAYER_FLAGS_GM_TAG;
  if (gm.dndOn)
    pf |= PLAYER_FLAGS_DND;
  else
    pf &= ~PLAYER_FLAGS_DND;
  if (gm.devTagOn)
    pf |= PLAYER_FLAGS_DEVELOPER;
  else
    pf &= ~PLAYER_FLAGS_DEVELOPER;
  fields[static_cast<uint16>(PLAYER_FLAGS)] = pf;

  uint32 uf = 0;
  if (auto it = fields.find(static_cast<uint16>(UNIT_FIELD_FLAGS));
      it != fields.end()) {
    uf = it->second;
  }
  if (!gm.visibleToOthers)
    uf |= UNIT_FLAG_INVISIBLE;
  else
    uf &= ~UNIT_FLAG_INVISIBLE;
  fields[static_cast<uint16>(UNIT_FIELD_FLAGS)] = uf;
}

uint32 ComputePlayerWireUnitFieldFlags(PlayerGmAppearanceForUpdates const &gm,
                                       bool inCombat) {
  uint32 uf = kUnitFlagCanSwim;
  if (inCombat)
    uf |= kUnitFlagInCombat;
  if (!gm.visibleToOthers)
    uf |= UNIT_FLAG_INVISIBLE;
  return uf;
}

} // namespace Firelands
