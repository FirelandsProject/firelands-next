#include <infrastructure/world/MapCreatureProximityAggroTicker.h>

#include <application/combat/CreatureProximityAggro.h>
#include <application/services/WorldService.h>
#include <domain/world/Creature.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/network/MovementInfo.h>
#include <vector>

namespace Firelands {

namespace {

constexpr std::chrono::milliseconds kProximityAggroTickInterval{500};

} // namespace

void TickMapCreatureProximityAggro(std::chrono::steady_clock::time_point now) {
  static std::chrono::steady_clock::time_point lastTick{};
  if (lastTick.time_since_epoch().count() != 0 && now - lastTick < kProximityAggroTickInterval)
    return;
  lastTick = now;

  std::shared_ptr<FactionTemplateDbc const> factionTemplates =
      WorldService::Instance().GetFactionTemplateDbc();
  if (!factionTemplates || !factionTemplates->IsLoaded())
    return;

  WorldService::Instance().ForEachMap(
      [&](uint32 /*mapId*/, std::shared_ptr<Map> const &map) {
        std::vector<application::ProximityAggroPlayer> players;
        map->ForEachPlayer([&](std::shared_ptr<Player> const &player) {
          if (!player || player->GetLiveHealth() == 0)
            return;
          application::ProximityAggroPlayer scan{};
          scan.guid = player->GetGuid();
          scan.player = player.get();
          if (auto const notifier = player->GetNotifier()) {
            if (auto const session =
                    std::dynamic_pointer_cast<WorldSession>(notifier)) {
              MovementInfo const &pos = session->GetPosition();
              scan.x = pos.x;
              scan.y = pos.y;
              scan.z = pos.z;
            } else {
              scan.x = player->GetX();
              scan.y = player->GetY();
              scan.z = player->GetZ();
            }
          } else {
            scan.x = player->GetX();
            scan.y = player->GetY();
            scan.z = player->GetZ();
          }
          players.push_back(scan);
        });
        if (players.empty())
          return;

        map->ForEachCreature([&](std::shared_ptr<Creature> const &creature) {
          if (!creature)
            return;
          auto const targetGuid = application::PickClosestHostilePlayerInAggroRange(
              *creature, players, factionTemplates.get());
          if (!targetGuid)
            return;

          for (application::ProximityAggroPlayer const &scan : players) {
            if (scan.guid != *targetGuid)
              continue;
            if (!scan.player)
              return;
            auto const notifier = scan.player->GetNotifier();
            if (!notifier)
              return;
            auto const session =
                std::dynamic_pointer_cast<WorldSession>(notifier);
            if (!session)
              return;
            session->StartCreatureAggro(creature->GetGuid());
            return;
          }
        });
      });
}

} // namespace Firelands
