#include <application/services/WorldService.h>

namespace Firelands {

void WorldService::AddCreatureToMap(uint32 mapId,
                                    std::shared_ptr<Creature> creature) {
  const uint64 guid = creature->GetGuid();
  GetMap(mapId)->AddObject(std::move(creature));
  if (auto h = GetScriptHost()) {
    h->FireEvent("creature_spawn", guid);
  }
}

void WorldService::AddGameObjectToMap(uint32 mapId,
                                      std::shared_ptr<GameObject> object) {
  const uint64 guid = object->GetGuid();
  GetMap(mapId)->AddObject(std::move(object));
  if (auto h = GetScriptHost()) {
    h->FireEvent("gameobject_spawn", guid);
  }
}

void WorldService::SetScriptHost(std::shared_ptr<IGameScriptHost> host) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_scriptHost = std::move(host);
}

std::shared_ptr<IGameScriptHost> WorldService::GetScriptHost() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_scriptHost;
}

void WorldService::SetCollisionQueries(
    std::shared_ptr<IMapCollisionQueries> queries) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_collisionQueries = std::move(queries);
}

std::shared_ptr<IMapCollisionQueries> WorldService::GetCollisionQueries() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_collisionQueries;
}

void WorldService::ResetForShutdown() {
  std::unordered_map<uint32, std::shared_ptr<Map>> mapsToDestroy;
  {
    std::lock_guard<std::mutex> lock(m_worldMutex);
    mapsToDestroy.swap(m_maps);
  }
  {
    std::lock_guard<std::mutex> lock(m_auxMutex);
    m_scriptHost.reset();
    m_collisionQueries.reset();
  }
  mapsToDestroy.clear();
}

} // namespace Firelands
