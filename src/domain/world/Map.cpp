#include <algorithm>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <shared/Logger.h>
#include <shared/network/WorldPacket.h>

namespace Firelands {

static constexpr float GRID_SIZE = 533.33333f;
static constexpr float MAX_COORD = 32.0f * GRID_SIZE;

inline int ComputeGridCoord(float pos) {
  int coord = static_cast<int>(32.0f - (pos / GRID_SIZE));
  return std::clamp(coord, 0, 63);
}

Map::GridCoord Map::GetCoord(float x, float y) {
  return {ComputeGridCoord(y), ComputeGridCoord(x)};
}

void Map::AddObject(std::shared_ptr<WorldObject> obj) {
  std::lock_guard<std::mutex> lock(m_mapMutex);
  m_objects[obj->GetGuid()] = obj;

  GridCoord coord = GetCoord(obj->GetX(), obj->GetY());
  m_grid[coord.x][coord.y].objects[obj->GetGuid()] = obj;
  m_objectCoords[obj->GetGuid()] = coord;

  LOG_INFO("Object {} added to Map {} at Cell [{}, {}]", obj->GetGuid(), m_id,
           coord.x, coord.y);
}

void Map::RemoveObject(uint64 guid) {
  std::lock_guard<std::mutex> lock(m_mapMutex);

  auto it = m_objectCoords.find(guid);
  if (it != m_objectCoords.end()) {
    m_grid[it->second.x][it->second.y].objects.erase(guid);
    m_objectCoords.erase(it);
  }

  m_objects.erase(guid);
  LOG_INFO("Object {} removed from Map {}", guid, m_id);
}

void Map::UpdateObjectPosition(uint64 guid, const MovementInfo &pos) {
  std::lock_guard<std::mutex> lock(m_mapMutex);
  auto it = m_objects.find(guid);
  if (it == m_objects.end())
    return;

  std::shared_ptr<WorldObject> obj = it->second;
  GridCoord oldCoord = m_objectCoords[guid];
  GridCoord newCoord = GetCoord(pos.x, pos.y);

  obj->SetPosition(pos);

  if (!(oldCoord == newCoord)) {
    m_grid[oldCoord.x][oldCoord.y].objects.erase(guid);
    m_grid[newCoord.x][newCoord.y].objects[guid] = obj;
    m_objectCoords[guid] = newCoord;
    LOG_TRACE("Object {} crossed to Cell [{}, {}]", guid, newCoord.x,
              newCoord.y);
  }
}

void Map::ForEachPlayer(
    std::function<void(std::shared_ptr<Player> const &)> const &fn) {
  std::lock_guard<std::mutex> lock(m_mapMutex);
  for (auto const &[id, obj] : m_objects) {
    (void)id;
    if (auto pl = std::dynamic_pointer_cast<Player>(obj))
      fn(pl);
  }
}

void Map::BroadcastPacket(uint64 senderGuid, WorldPacket &packet,
                          bool includeSelf) {
  std::lock_guard<std::mutex> lock(m_mapMutex);

  for (auto const &[guid, obj] : m_objects) {
    if (!includeSelf && guid == senderGuid)
      continue;

    if (auto player = std::dynamic_pointer_cast<Player>(obj)) {
      if (auto notifier = player->GetNotifier()) {
        notifier->SendPacket(packet);
      }
    }
  }
}

void Map::BroadcastPacketToNearby(uint64 senderGuid, WorldPacket &packet,
                                  bool includeSelf) {
  std::lock_guard<std::mutex> lock(m_mapMutex);
  auto it = m_objects.find(senderGuid);
  if (it == m_objects.end())
    return;

  std::shared_ptr<WorldObject> sender = it->second;
  GridCoord center = GetCoord(sender->GetX(), sender->GetY());

  for (int x = center.x - 1; x <= center.x + 1; ++x) {
    for (int y = center.y - 1; y <= center.y + 1; ++y) {
      if (x < 0 || x >= 64 || y < 0 || y >= 64)
        continue;

      for (auto const &[guid, obj] : m_grid[x][y].objects) {
        if (!includeSelf && guid == senderGuid)
          continue;

        if (auto player = std::dynamic_pointer_cast<Player>(obj)) {
          if (auto notifier = player->GetNotifier()) {
            notifier->SendPacket(packet);
          }
        }
      }
    }
  }
}

} // namespace Firelands
