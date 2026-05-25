#include "CombatHostility.h"

#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/game/PlayerFactionTeam.h>

namespace application {

bool CanMeleeAttack(Firelands::Player const &attacker, Firelands::Player const &target) {
  if (attacker.GetGuid() == target.GetGuid())
    return false;
  bool sameTeam = false;
  if (Firelands::TrySpellRangeFriendlyTeamHint(attacker.GetRace(), target.GetRace(),
                                               &sameTeam)) {
    return !sameTeam;
  }
  return attacker.GetFactionTemplate() != target.GetFactionTemplate();
}

bool CanMeleeAttack(Firelands::Player const &attacker, Firelands::Creature const &target,
                    Firelands::FactionTemplateDbc const *factionTemplates) {
  (void)attacker;
  if (target.IsEvading())
    return false;
  if (!factionTemplates || !factionTemplates->IsLoaded())
    return true;

  auto const playerTpl = factionTemplates->TryGet(attacker.GetFactionTemplate());
  auto const creatureTpl = factionTemplates->TryGet(target.GetFactionTemplate());
  if (!playerTpl || !creatureTpl)
    return true;

  for (uint32_t enemyFaction : playerTpl->enemies) {
    if (enemyFaction != 0 && enemyFaction == creatureTpl->faction)
      return true;
  }
  for (uint32_t enemyFaction : creatureTpl->enemies) {
    if (enemyFaction != 0 && enemyFaction == playerTpl->faction)
      return true;
  }

  if (playerTpl->factionGroup != 0 && creatureTpl->factionGroup != 0 &&
      playerTpl->factionGroup == creatureTpl->factionGroup)
    return false;

  return playerTpl->faction != creatureTpl->faction;
}

} // namespace application
