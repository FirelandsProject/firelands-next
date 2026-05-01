#include "RealmListService.h"

namespace Firelands {

namespace {

/// Trinity-compatible AUTH realm row flag (realm not accepting connections).
constexpr uint8_t kAuthRealmListFlagOffline = 2;

} // namespace

RealmListService::RealmListService(std::shared_ptr<IRealmRepository> repository,
                                   std::shared_ptr<IRealmLiveState> liveState)
    : m_repository(std::move(repository)), m_liveState(std::move(liveState)) {}

std::vector<Realm> RealmListService::GetRealmList() {
  if (!m_repository) {
    return {};
  }
  std::vector<Realm> realms = m_repository->GetRealms();
  if (!m_liveState)
    return realms;

  std::vector<Realm> out;
  out.reserve(realms.size());
  for (Realm const &r : realms) {
    if (m_liveState->IsWorldConnected(static_cast<uint32_t>(r.GetId())))
      out.push_back(r);
    else
      out.emplace_back(static_cast<uint32_t>(r.GetId()), r.GetName(),
                       r.GetAddress(), r.GetPort(), r.GetIcon(), r.GetTimezone(),
                       r.GetAllowedSecurityLevel(), 0.0f,
                       kAuthRealmListFlagOffline);
  }
  return out;
}

} // namespace Firelands
