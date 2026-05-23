#pragma once

#include <shared/Common.h>
#include <cstdint>

namespace Firelands {

/// Static creature placement from `creature` + `creature_template` (domain shape).
struct CreatureSpawn {
  uint64 guid = 0;
  uint32 entry = 0;
  uint32 mapId = 0;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float orientation = 0.f;
  uint32 modelId = 0;
  uint32 templateModelId1 = 0;
  uint32 templateModelId2 = 0;
  uint32 templateModelId3 = 0;
  uint32 templateModelId4 = 0;
  uint8 unitClass = 0;
  uint8 minLevel = 1;
  uint8 maxLevel = 1;
  uint32 factionTemplate = 0;
  uint32 npcFlags = 0;
  uint32 unitFieldFlags = 0;
  uint32 unitFieldFlags2 = 0;
  uint32 extraFlags = 0;
  float experienceModifier = 1.0f;
  uint8 phaseUseFlags = 0;
  uint16 phaseId = 0;
  uint32 phaseGroup = 0;
};

} // namespace Firelands
