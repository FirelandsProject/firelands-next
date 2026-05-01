#pragma once

#include <shared/Common.h>

namespace Firelands {

/// Runtime “world connected for realm X” signal (fed by auth’s realm-link TCP).
class IRealmLiveState {
public:
  virtual ~IRealmLiveState() = default;
  virtual bool IsWorldConnected(uint32_t realmId) const = 0;
};

} // namespace Firelands
