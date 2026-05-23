#include <application/combat/CombatHostility.h>
#include <application/combat/CreatureProximityAggro.h>

#include <cmath>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <limits>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/dbc/FactionTemplateHelpers.h>

namespace application {

namespace {

constexpr float kProximityAggroMaxVerticalYards = 10.0f;

float DistanceSquared2d(float ax, float ay, float bx, float by) {
  float const dx = ax - bx;
  float const dy = ay - by;
  return dx * dx + dy * dy;
}

} // namespace

bool FactionProvokesProximityAggro(Firelands::FactionTemplateEntry const &entry) {
  if (FactionTemplateLikelyHostileToPlayers(entry))
    return true;
  return (entry.factionGroup & Firelands::FactionGroupMaskMonster) != 0;
}

bool IsWithinProximityAggroRange(float creatureX, float creatureY, float creatureZ,
                                 float playerX, float playerY, float playerZ,
                                 float rangeYards) {
  float const rangeSq = rangeYards * rangeYards;
  if (DistanceSquared2d(creatureX, creatureY, playerX, playerY) > rangeSq)
    return false;
  return std::fabs(creatureZ - playerZ) <= kProximityAggroMaxVerticalYards;
}

std::optional<uint64_t> PickClosestHostilePlayerInAggroRange(
    Firelands::Creature const &creature, std::vector<ProximityAggroPlayer> const &players,
    Firelands::FactionTemplateDbc const *factionTemplates) {
  if (creature.GetLiveHealth() == 0 || creature.IsEvading())
    return std::nullopt;
  if (creature.GetChaseTargetPlayerGuid() != 0)
    return std::nullopt;
  if (!factionTemplates || !factionTemplates->IsLoaded())
    return std::nullopt;

  auto const creatureTpl = factionTemplates->TryGet(creature.GetFactionTemplate());
  if (!creatureTpl || !FactionProvokesProximityAggro(*creatureTpl))
    return std::nullopt;

  float bestDistSq = std::numeric_limits<float>::max();
  std::optional<uint64_t> bestGuid;

  for (ProximityAggroPlayer const &scan : players) {
    if (!scan.player || scan.player->GetLiveHealth() == 0)
      continue;
    if (!CanMeleeAttack(*scan.player, creature, factionTemplates))
      continue;
    if (!IsWithinProximityAggroRange(creature.GetX(), creature.GetY(), creature.GetZ(),
                                     scan.x, scan.y, scan.z))
      continue;

    float const distSq =
        DistanceSquared2d(creature.GetX(), creature.GetY(), scan.x, scan.y);
    if (distSq < bestDistSq) {
      bestDistSq = distSq;
      bestGuid = scan.guid;
    }
  }

  return bestGuid;
}

} // namespace application
