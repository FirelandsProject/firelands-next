#pragma once

#include <shared/Common.h>
#include <cstdint>

namespace Firelands {

class IGmFactionCommandPort {
public:
  virtual ~IGmFactionCommandPort() = default;
  virtual bool GmSetForcedFactionReaction(uint32 factionDbcId, uint8 reputationRank) {
    (void)factionDbcId;
    (void)reputationRank;
    return false;
  }
  virtual bool GmClearForcedFactionReaction(uint32 factionDbcId) {
    (void)factionDbcId;
    return false;
  }
  virtual bool GmClearAllForcedFactionReactions() { return false; }
  virtual bool GmSetOwnFactionTemplate(uint32 factionTemplate) {
    (void)factionTemplate;
    return false;
  }
  virtual bool GmSetSelectedCreatureFactionTemplate(uint32 factionTemplate) {
    (void)factionTemplate;
    return false;
  }
};

} // namespace Firelands
