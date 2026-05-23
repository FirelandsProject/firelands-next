#include <application/gm/GmPlayerRevive.h>

#include <domain/world/Player.h>

namespace Firelands {

GmReviveOutcome ApplyGmReviveToPlayer(Player &pl) {
  GmReviveOutcome outcome;
  uint32 const maxHp = pl.GetLiveMaxHealth();
  uint32 const maxPow = pl.GetLiveMaxPower1();
  uint32 const hpBefore = pl.GetLiveHealth();
  uint32 const powBefore = pl.GetLivePower1();

  if (hpBefore >= maxHp && powBefore >= maxPow) {
    outcome.result = GmReviveResult::AlreadyFull;
    return outcome;
  }

  if (hpBefore < maxHp) {
    pl.ApplyHealthDelta(static_cast<int32>(maxHp - hpBefore));
    outcome.healthChanged = true;
  }

  if (powBefore < maxPow) {
    pl.ApplyPower1Delta(static_cast<int32>(maxPow - powBefore));
    outcome.powerChanged = true;
  }

  if (hpBefore == 0)
    outcome.result = GmReviveResult::RevivedFromDeath;
  else
    outcome.result = GmReviveResult::Restored;
  return outcome;
}

} // namespace Firelands
