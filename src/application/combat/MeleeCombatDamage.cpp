#include <application/combat/MeleeCombatDamage.h>

#include <application/combat/CombatEntityAdapters.h>
#include <shared/game/CombatDamage.h>

namespace application {

using Firelands::ComputeMeleeSwingDamage;
using Firelands::UnitCombatStats;

Firelands::uint32 ComputeMeleeDamageBetween(::combat::ICombatEntity const &attacker,
                                            ::combat::ICombatEntity const &victim) {
  UnitCombatStats const *attackerStats = nullptr;
  UnitCombatStats const *victimStats = nullptr;

  if (auto const *playerAttacker =
          dynamic_cast<adapters::PlayerCombatEntity const *>(&attacker)) {
    if (auto pl = playerAttacker->GetPlayer())
      attackerStats = &pl->GetCombatStats();
  } else if (auto const *creatureAttacker =
                 dynamic_cast<adapters::CreatureCombatEntity const *>(&attacker)) {
    if (auto cr = creatureAttacker->GetCreature())
      attackerStats = &cr->GetCombatStats();
  }

  if (auto const *playerVictim =
          dynamic_cast<adapters::PlayerCombatEntity const *>(&victim)) {
    if (auto pl = playerVictim->GetPlayer())
      victimStats = &pl->GetCombatStats();
  } else if (auto const *creatureVictim =
                 dynamic_cast<adapters::CreatureCombatEntity const *>(&victim)) {
    if (auto cr = creatureVictim->GetCreature())
      victimStats = &cr->GetCombatStats();
  }

  if (!attackerStats || !victimStats)
    return 10u;

  return ComputeMeleeSwingDamage(*attackerStats, *victimStats);
}

} // namespace application
