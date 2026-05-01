#pragma once

#include <application/ports/IRealmLiveState.h>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Firelands {

/// Thread-safe registry: at most one live realm-link per realm id.
class RealmLiveRegistry : public IRealmLiveState {
public:
  enum class ClaimResult { Ok, AlreadyOnline };

  bool IsWorldConnected(uint32_t realmId) const override;

  ClaimResult tryClaim(uint32_t realmId);
  void release(uint32_t realmId);

private:
  mutable std::mutex m_mutex;
  std::unordered_map<uint32_t, bool> m_connected;
};

} // namespace Firelands
