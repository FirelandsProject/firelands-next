#include <application/services/MapRegistry.h>

namespace Firelands {

std::shared_ptr<MapService> MapRegistry::GetOrCreate(uint32 mapId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_services.find(mapId);
  if (it != m_services.end())
    return it->second;
  auto map = std::make_shared<Map>(mapId);
  auto service = std::make_shared<MapService>(mapId, map);
  m_services.emplace(mapId, service);
  return service;
}

std::shared_ptr<MapService> MapRegistry::TryGet(uint32 mapId) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_services.find(mapId);
  if (it == m_services.end())
    return nullptr;
  return it->second;
}

void MapRegistry::ForEach(std::function<void(MapService &)> const &fn) {
  std::vector<std::shared_ptr<MapService>> services;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    services.reserve(m_services.size());
    for (auto const &[id, svc] : m_services) {
      (void)id;
      if (svc)
        services.push_back(svc);
    }
  }
  for (auto const &svc : services) {
    if (svc)
      fn(*svc);
  }
}

std::vector<MapSnapshot> MapRegistry::AllSnapshots() const {
  std::vector<MapSnapshot> out;
  std::lock_guard<std::mutex> lock(m_mutex);
  out.reserve(m_services.size());
  for (auto const &[id, svc] : m_services) {
    (void)id;
    if (svc)
      out.push_back(svc->Snapshot());
  }
  return out;
}

std::size_t MapRegistry::MapCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_services.size();
}

void MapRegistry::Clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_services.clear();
}

} // namespace Firelands
