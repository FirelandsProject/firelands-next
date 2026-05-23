#pragma once

#include <domain/world/Creature.h>
#include <domain/world/GameObject.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <shared/game/ExperienceRates.h>
#include <functional>
#include <memory>
#include <cstdint>

namespace Firelands {
class IGameScriptHost;
class IMapCollisionQueries;
class ISpellCastTables;
class ISpellDefinitionStore;
class SpellVisualDbc;
class FactionTemplateDbc;
class AreaTableDbc;
class PhaseGroupCatalog;
class PhaseAreaCatalog;

/// Port for world-server runtime state (maps, catalogs, DBC holders).
class IWorldRuntime {
public:
  virtual ~IWorldRuntime() = default;

  virtual std::shared_ptr<Map> GetMap(uint32 mapId) = 0;
  virtual void AddPlayerToMap(uint32 mapId, std::shared_ptr<Player> player) = 0;
  virtual void RemovePlayerFromMap(uint32 mapId, uint64 guid) = 0;
  virtual void AddCreatureToMap(uint32 mapId, std::shared_ptr<Creature> creature) = 0;
  virtual void AddGameObjectToMap(uint32 mapId, std::shared_ptr<GameObject> object) = 0;
  virtual void ForEachMap(
      std::function<void(uint32 mapId, std::shared_ptr<Map> const &)> const &fn) = 0;

  virtual std::shared_ptr<IGameScriptHost> GetScriptHost() = 0;
  virtual std::shared_ptr<IMapCollisionQueries> GetCollisionQueries() = 0;
  virtual std::shared_ptr<ISpellCastTables const> GetSpellCastTables() = 0;
  virtual std::shared_ptr<ISpellDefinitionStore const> GetSpellDefinitions() = 0;
  virtual std::shared_ptr<SpellVisualDbc const> GetSpellVisualDbc() = 0;
  virtual std::shared_ptr<FactionTemplateDbc const> GetFactionTemplateDbc() = 0;
  virtual std::shared_ptr<PhaseGroupCatalog const> GetPhaseGroupCatalog() = 0;
  virtual std::shared_ptr<PhaseAreaCatalog const> GetPhaseAreaCatalog() = 0;
  virtual std::shared_ptr<AreaTableDbc const> GetAreaTableDbc() = 0;
  virtual ExperienceRates GetExperienceRates() = 0;
};

} // namespace Firelands
