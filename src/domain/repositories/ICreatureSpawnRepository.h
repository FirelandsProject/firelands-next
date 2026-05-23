#pragma once

#include <shared/Common.h>
#include <cstdint>
#include <vector>

namespace Firelands {

struct CreatureSpawnRow {
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
  /// `creature_template.faction` → `FactionTemplate.dbc` id.
  uint32 factionTemplate = 0;
  /// `creature_template.npcflag` → `UNIT_NPC_FLAGS` on spawn.
  uint32 npcFlags = 0;
  /// `creature_template.ExperienceModifier` (1.0 = normal).
  float experienceModifier = 1.0f;
  uint8 phaseUseFlags = 0;
  uint16 phaseId = 0;
  uint32 phaseGroup = 0;
};

/// Loads static NPC placements from `creature` joined with `creature_template`.
class ICreatureSpawnRepository {
public:
  virtual ~ICreatureSpawnRepository() = default;

  virtual std::vector<CreatureSpawnRow> LoadAllSpawns() const = 0;
};

} // namespace Firelands
