#pragma once

#include <application/ports/IRealmLiveState.h>
#include <domain/models/Realm.h>
#include <domain/repositories/IRealmRepository.h>
#include <memory>
#include <vector>

namespace Firelands {

class RealmListService {
public:
  explicit RealmListService(
      std::shared_ptr<IRealmRepository> repository,
      std::shared_ptr<IRealmLiveState> liveState = nullptr);

  std::vector<Realm> GetRealmList();

  /// True when auth was started with a `RealmLiveRegistry` (RealmLink.Token set).
  bool UsesLiveRealmState() const noexcept { return static_cast<bool>(m_liveState); }

private:
  std::shared_ptr<IRealmRepository> m_repository;
  std::shared_ptr<IRealmLiveState> m_liveState;
};

} // namespace Firelands
