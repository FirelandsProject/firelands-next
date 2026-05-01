#include <infrastructure/network/realm_link/RealmLiveRegistry.h>

namespace Firelands {

bool RealmLiveRegistry::IsWorldConnected(uint32_t realmId) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_connected.find(realmId);
  return it != m_connected.end() && it->second;
}

RealmLiveRegistry::ClaimResult RealmLiveRegistry::tryClaim(uint32_t realmId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_connected.find(realmId);
  if (it != m_connected.end() && it->second)
    return ClaimResult::AlreadyOnline;
  m_connected[realmId] = true;
  return ClaimResult::Ok;
}

void RealmLiveRegistry::release(uint32_t realmId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_connected.erase(realmId);
}

} // namespace Firelands
