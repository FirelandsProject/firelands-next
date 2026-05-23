#include <shared/game/RestExperienceLogic.h>

#include <algorithm>

namespace Firelands::RestExperienceLogic {

float RestBonusCap(uint32_t nextLevelXp) {
  if (nextLevelXp == 0)
    return 0.f;
  return static_cast<float>(nextLevelXp) * 1.5f / 2.f;
}

float ClampRestBonus(float restBonus, uint32_t nextLevelXp) {
  if (restBonus < 0.f)
    restBonus = 0.f;
  float const cap = RestBonusCap(nextLevelXp);
  if (restBonus > cap)
    restBonus = cap;
  return restBonus;
}

RestConsumeResult ConsumeForKill(float restBonus, uint32_t baseXp,
                                 uint32_t nextLevelXp) {
  RestConsumeResult result;
  restBonus = ClampRestBonus(restBonus, nextLevelXp);
  if (baseXp == 0 || restBonus <= 0.f)
    return result;

  uint32_t const pool = static_cast<uint32_t>(restBonus);
  uint32_t bonus = pool;
  if (bonus > baseXp)
    bonus = baseXp;

  result.restedBonus = bonus;
  result.restBonusAfter = restBonus - static_cast<float>(bonus);
  if (result.restBonusAfter < 0.f)
    result.restBonusAfter = 0.f;
  return result;
}

RestStateWire RestStateForBonus(float restBonus) {
  if (restBonus > 10.f)
    return RestStateWire::Rested;
  return RestStateWire::NotRafLinked;
}

uint32_t RestBonusWireAmount(float restBonus) {
  if (restBonus <= 0.f)
    return 0u;
  return static_cast<uint32_t>(restBonus);
}

} // namespace Firelands::RestExperienceLogic
