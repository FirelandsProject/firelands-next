#pragma once

#include <shared/Common.h>
#include <cstdint>

namespace Firelands {

class IGmUnitCommandPort {
public:
  virtual ~IGmUnitCommandPort() = default;
  virtual bool GmDamageUnit(uint64 targetGuid, uint32 amount) {
    (void)targetGuid;
    (void)amount;
    return false;
  }
  virtual bool GmSpawnNpc(uint32 creatureEntry, uint32 displayId,
                          uint32 factionTemplateOrZeroDefault = 0) {
    (void)creatureEntry;
    (void)displayId;
    (void)factionTemplateOrZeroDefault;
    return false;
  }
  virtual bool GmDeleteNpcByObjectGuid(uint64 objectGuid) {
    (void)objectGuid;
    return false;
  }
};

} // namespace Firelands
