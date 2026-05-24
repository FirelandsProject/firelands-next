#pragma once

#include <application/services/MapService.h>
#include <domain/world/MapSnapshot.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Firelands {

class MapRegistry {
public:
  std::shared_ptr<MapService> GetOrCreate(uint32 mapId);
  std::shared_ptr<MapService> TryGet(uint32 mapId) const;
  void ForEach(std::function<void(MapService &)> const &fn);
  std::vector<MapSnapshot> AllSnapshots() const;
  std::size_t MapCount() const;
  void Clear();

private:
  mutable std::mutex m_mutex;
  std::unordered_map<uint32, std::shared_ptr<MapService>> m_services;
};

} // namespace Firelands
