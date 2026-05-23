#include "CombatService.h"

#include <application/combat/MeleeCombatDamage.h>

namespace application {

void CombatService::StartCombat(::combat::ICombatEntity &attacker,
                                ::combat::ICombatEntity &victim) {
  _engine->Engage(attacker, victim);
}

MeleeSwingResult CombatService::BeginMeleeSwing(::combat::ICombatEntity &attacker,
                                                ::combat::ICombatEntity &victim) {
  if (attacker.GetGuid() == 0 || victim.GetGuid() == 0)
    return MeleeSwingResult::InvalidTarget;
  if (!victim.IsAlive())
    return MeleeSwingResult::DeadTarget;
  if (!attacker.IsAlive())
    return MeleeSwingResult::CantAttack;

  StartCombat(attacker, victim);
  Firelands::uint32 const damage = ComputeMeleeDamageBetween(attacker, victim);
  _engine->ApplyMeleeDamage(victim, static_cast<float>(damage));
  return MeleeSwingResult::Success;
}

MeleeSwingResult CombatService::ApplyMeleeHit(::combat::ICombatEntity &attacker,
                                              ::combat::ICombatEntity &victim) {
  if (attacker.GetGuid() == 0 || victim.GetGuid() == 0)
    return MeleeSwingResult::InvalidTarget;
  if (!victim.IsAlive())
    return MeleeSwingResult::DeadTarget;
  if (!attacker.IsAlive())
    return MeleeSwingResult::CantAttack;

  Firelands::uint32 const damage = ComputeMeleeDamageBetween(attacker, victim);
  _engine->ApplyMeleeDamage(victim, static_cast<float>(damage));
  return MeleeSwingResult::Success;
}

} // namespace application
