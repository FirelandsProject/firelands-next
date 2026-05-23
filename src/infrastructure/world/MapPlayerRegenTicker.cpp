#include <infrastructure/world/MapPlayerRegenTicker.h>

#include <application/world/WorldRuntimeAccess.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/PlayerPowerType.h>
#include <shared/game/PlayerResourceRegen.h>
#include <shared/network/UpdateData.h>
#include <shared/network/packets/server/CombatPackets.h>

namespace Firelands {

namespace {

constexpr std::chrono::milliseconds kRegenTickInterval{2000};

void BroadcastPlayerRegenUpdates(uint32 mapId, std::shared_ptr<Map> const &map,
                                 uint64 playerGuid, uint8 powerType,
                                 uint32 health, uint32 maxHealth, uint32 power1,
                                 uint32 maxPower1) {
  WorldPacket hpUpdate;
  WorldSessionObjectUpdate::BuildPlayerHealthValuesUpdate(
      static_cast<uint16>(mapId), playerGuid, health, maxHealth, hpUpdate);
  map->BroadcastPacketToNearby(playerGuid, hpUpdate, true);

  WorldPacket powerUpdate =
      combat_wire::BuildPowerUpdate(playerGuid, powerType, power1);
  map->BroadcastPacketToNearby(playerGuid, powerUpdate, true);
  (void)maxPower1;
}

} // namespace

void TickMapPlayerResourceRegen(std::chrono::steady_clock::time_point now) {
  static std::chrono::steady_clock::time_point lastTick{};
  if (lastTick.time_since_epoch().count() != 0 &&
      now - lastTick < kRegenTickInterval)
    return;
  std::chrono::milliseconds const interval =
      lastTick.time_since_epoch().count() == 0
          ? kRegenTickInterval
          : std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);
  lastTick = now;

  WorldRuntime().ForEachMap(
      [&](uint32 mapId, std::shared_ptr<Map> const &map) {
        map->ForEachPlayer([&](std::shared_ptr<Player> const &player) {
          if (!player || player->GetLiveHealth() == 0)
            return;

          bool const outOfCombat = player->IsOutOfCombatForRegen(now);
          PlayerPowerType const pt =
              static_cast<PlayerPowerType>(player->GetPowerType());

          ResourceRegenDelta const delta = ComputeResourceRegenDelta(
              pt, player->GetLevel(), player->GetSpirit(), player->GetLiveHealth(),
              player->GetLiveMaxHealth(), player->GetLivePower1(),
              player->GetLiveMaxPower1(), outOfCombat, interval,
              player->GetResourceRegenModifiers());

          if (delta.health == 0 && delta.power1 == 0)
            return;

          if (delta.health != 0)
            player->ApplyHealthDelta(delta.health);
          if (delta.power1 != 0)
            player->ApplyPower1Delta(delta.power1);

          if (auto notifier = player->GetNotifier()) {
            WorldPacket hpSelf;
            WorldSessionObjectUpdate::BuildPlayerHealthValuesUpdate(
                static_cast<uint16>(mapId), player->GetGuid(),
                player->GetLiveHealth(), player->GetLiveMaxHealth(), hpSelf);
            notifier->SendPacket(hpSelf);
            WorldPacket powerSelf =
                combat_wire::BuildPowerUpdate(player->GetGuid(),
                                              static_cast<uint8>(pt),
                                              player->GetLivePower1());
            notifier->SendPacket(powerSelf);
          }
          BroadcastPlayerRegenUpdates(mapId, map, player->GetGuid(),
                                      static_cast<uint8>(pt),
                                      player->GetLiveHealth(),
                                      player->GetLiveMaxHealth(),
                                      player->GetLivePower1(),
                                      player->GetLiveMaxPower1());
        });
      });
}

} // namespace Firelands
