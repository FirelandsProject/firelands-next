#pragma once

#include <domain/world/MapSnapshot.h>
#include <domain/world/Player.h>
#include <domain/world/WorldObject.h>
#include <functional>
#include <memory>
#include <mutex>
#include <shared/Common.h>
#include <unordered_map>

namespace Firelands {

class WorldSession;
class Creature;

class Map {
public:
  explicit Map(uint32 id) : m_id(id) {}

  uint32 GetMapId() const { return m_id; }

  void AddObject(std::shared_ptr<WorldObject> obj);
  void RemoveObject(uint64 guid);

  void UpdateObjectPosition(uint64 guid, const MovementInfo &pos);

  /// Thread-safe lookup for spell range / proximity (players, creatures, GOs on map).
  bool TryGetObjectWorldPosition(uint64 guid, float &outX, float &outY, float &outZ);

  void BroadcastPacket(uint64 senderGuid, class WorldPacket &packet,
                       bool includeSelf = false);
  void BroadcastPacketToNearby(std::shared_ptr<WorldObject> sender,
                               class WorldPacket &packet,
                               bool includeSelf = false);
  void BroadcastPacketToNearby(uint64 senderGuid, class WorldPacket &packet,
                               bool includeSelf = false);

  void ForEachPlayer(
      std::function<void(std::shared_ptr<Player> const &)> const &fn);

  void ForEachCreature(
      std::function<void(std::shared_ptr<Creature> const &)> const &fn);

  /// Returns the player on this map, or nullptr if missing or not a player.
  std::shared_ptr<Player> TryGetPlayer(uint64 guid);

  /// Returns the creature on this map, or nullptr if missing or not a creature.
  std::shared_ptr<Creature> TryGetCreature(uint64 guid);

  /// Updates rolling average tick duration (exponential moving average, alpha=0.1).
  void RecordTickTime(double ms);

  /// Thread-safe snapshot of occupancy and tick timing.
  MapSnapshot CreateSnapshot() const;

  /// True when the map has no players and no creatures (game objects ignored).
  bool IsEmpty() const;

  /// Player/creature counts without scanning the spatial grid (for TUI watches).
  int CountPlayers() const;
  int CountCreatures() const;

  /// Invokes `fn` for each `Creature` in grid cells within `cellRadius` of (x,y).
  void ForEachCreatureNear(
      float x, float y, int cellRadius,
      std::function<void(std::shared_ptr<Creature> const &)> const &fn);

private:
  struct GridCoord {
    int x, y;
    bool operator==(const GridCoord &other) const {
      return x == other.x && y == other.y;
    }
  };

  struct Cell {
    std::unordered_map<uint64, std::shared_ptr<WorldObject>> objects;
  };

  GridCoord GetCoord(float x, float y);

  uint32 m_id;
  std::unordered_map<uint64, std::shared_ptr<WorldObject>> m_objects;
  std::unordered_map<uint64, GridCoord> m_objectCoords;
  Cell m_grid[64][64];
  mutable std::mutex m_mapMutex;
  double m_avgTickTimeMs = 0.0;
  double m_lastTickTimeMs = 0.0;
  bool m_hasTickSample = false;
};

} // namespace Firelands
