#pragma once

#include <domain/models/Realm.h>
#include <cstdint>
#include <optional>
#include <vector>

namespace Firelands {

class IRealmRepository {
public:
  virtual ~IRealmRepository() = default;

  virtual bool FindById(uint32_t id) = 0;
  virtual void DeleteById(uint32_t id) = 0;
  virtual void Create(const Realm &realm) = 0;
  virtual std::vector<Realm> GetRealms() = 0;

  /// Minimum legacy staff tier (0–3, same scale as former `account.access_level`)
  /// required to join; checked against RBAC effective permission mask.
  /// Empty if the realm id is not present in `realmlist`.
  virtual std::optional<uint8_t> GetAllowedSecurityLevelForRealm(uint32_t id) = 0;
};

} // namespace Firelands
