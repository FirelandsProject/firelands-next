#pragma once

#include <application/ports/IGameScriptHost.h>
#include <application/ports/IMapCollisionQueries.h>
#include <domain/world/Creature.h>
#include <domain/world/GameObject.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Firelands {

/// Singleton world state (maps, shared script host, collision port).
/// Populated from `world` executable after config load.
class WorldService {
public:
  static WorldService &Instance() {
    static WorldService instance;
    return instance;
  }

  std::shared_ptr<Map> GetMap(uint32 mapId) {
    std::lock_guard<std::mutex> lock(m_worldMutex);
    auto it = m_maps.find(mapId);
    if (it == m_maps.end()) {
      m_maps[mapId] = std::make_shared<Map>(mapId);
    }
    return m_maps[mapId];
  }

  void AddPlayerToMap(uint32 mapId, std::shared_ptr<Player> player) {
    GetMap(mapId)->AddObject(player);
  }

  void RemovePlayerFromMap(uint32 mapId, uint64 guid) {
    std::shared_ptr<Map> map;
    {
      std::lock_guard<std::mutex> lock(m_worldMutex);
      auto it = m_maps.find(mapId);
      if (it == m_maps.end()) {
        return;
      }
      map = it->second;
    }
    map->RemoveObject(guid);
  }

  /// Adds a creature to the map grid and notifies Lua (`creature_spawn`).
  void AddCreatureToMap(uint32 mapId, std::shared_ptr<Creature> creature);

  /// Adds a game object to the map grid and notifies Lua (`gameobject_spawn`).
  void AddGameObjectToMap(uint32 mapId, std::shared_ptr<GameObject> object);

  void SetScriptHost(std::shared_ptr<IGameScriptHost> host);
  std::shared_ptr<IGameScriptHost> GetScriptHost();

  void SetCollisionQueries(std::shared_ptr<IMapCollisionQueries> queries);
  std::shared_ptr<IMapCollisionQueries> GetCollisionQueries();
  /// Explicit teardown hook for process shutdown. Releases map-held objects
  /// (players/sessions) while core services (io_context, logger) are still alive.
  void ResetForShutdown();

private:
  WorldService() = default;

  std::mutex m_worldMutex;
  std::unordered_map<uint32, std::shared_ptr<Map>> m_maps;

  std::mutex m_auxMutex;
  std::shared_ptr<IGameScriptHost> m_scriptHost;
  std::shared_ptr<IMapCollisionQueries> m_collisionQueries;
};

} // namespace Firelands
