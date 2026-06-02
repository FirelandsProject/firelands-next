#pragma once

#include <application/ports/IGameScriptHost.h>
#include <application/ports/IMapCollisionQueries.h>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Creature.h>
#include <domain/world/GameObject.h>
#include <application/services/MapRegistry.h>
#include <application/services/MapService.h>
#include <domain/world/Map.h>
#include <domain/world/MapSnapshot.h>
#include <domain/world/Player.h>
#include <shared/game/ExperienceRates.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace Firelands {

class SpellVisualDbc;
class FactionTemplateDbc;
class AreaTableDbc;
class PhaseGroupCatalog;
class PhaseAreaCatalog;
class INpcTemplateSearchRepository;

/// Singleton world state (maps, shared script host, collision port).
/// Populated from `world` executable after config load.
class WorldService {
public:
  static WorldService &Instance() {
    static WorldService instance;
    return instance;
  }

  std::shared_ptr<Map> GetMap(uint32 mapId);

  void AddPlayerToMap(uint32 mapId, std::shared_ptr<Player> player) {
    GetMap(mapId)->AddObject(player);
  }

  void RemovePlayerFromMap(uint32 mapId, uint64 guid);

  void RemoveCreatureFromMap(uint32 mapId, uint64 guid);

  /// Adds a creature to the map grid and notifies Lua (`creature_spawn`).
  void AddCreatureToMap(uint32 mapId, std::shared_ptr<Creature> creature);

  /// Adds a game object to the map grid and notifies Lua (`gameobject_spawn`).
  void AddGameObjectToMap(uint32 mapId, std::shared_ptr<GameObject> object);

  void SetScriptHost(std::shared_ptr<IGameScriptHost> host);
  std::shared_ptr<IGameScriptHost> GetScriptHost();

  void SetCollisionQueries(std::shared_ptr<IMapCollisionQueries> queries);
  std::shared_ptr<IMapCollisionQueries> GetCollisionQueries();

  void SetSpellCastTables(std::shared_ptr<ISpellCastTables const> tables);
  std::shared_ptr<ISpellCastTables const> GetSpellCastTables();
  void SetSpellDefinitions(std::shared_ptr<ISpellDefinitionStore const> definitions);
  std::shared_ptr<ISpellDefinitionStore const> GetSpellDefinitions();
  void SetSpellVisualDbc(std::shared_ptr<SpellVisualDbc const> spellVisualDbc);
  std::shared_ptr<SpellVisualDbc const> GetSpellVisualDbc();

  void SetFactionTemplateDbc(std::shared_ptr<FactionTemplateDbc const> factionTemplateDbc);
  std::shared_ptr<FactionTemplateDbc const> GetFactionTemplateDbc();

  void SetPhaseGroupCatalog(std::shared_ptr<PhaseGroupCatalog const> catalog);
  std::shared_ptr<PhaseGroupCatalog const> GetPhaseGroupCatalog();
  void SetPhaseAreaCatalog(std::shared_ptr<PhaseAreaCatalog const> catalog);
  std::shared_ptr<PhaseAreaCatalog const> GetPhaseAreaCatalog();

  void SetAreaTableDbc(std::shared_ptr<AreaTableDbc const> areaTableDbc);
  std::shared_ptr<AreaTableDbc const> GetAreaTableDbc();

  void SetNpcTemplateSearch(
      std::shared_ptr<INpcTemplateSearchRepository const> repo);
  std::shared_ptr<INpcTemplateSearchRepository const> GetNpcTemplateSearch();

  void SetExperienceRates(ExperienceRates rates);
  ExperienceRates GetExperienceRates();
  /// Explicit teardown hook for process shutdown. Releases map-held objects
  /// (players/sessions) while core services (io_context, logger) are still alive.
  void ResetForShutdown();

  /// Invokes `fn` for each active map while holding the maps lock.
  void ForEachMap(
      std::function<void(uint32 mapId, std::shared_ptr<Map> const &)> const &fn);

  void ForEachMapService(std::function<void(MapService &)> const &fn);

  std::vector<MapSnapshot> GetMapSnapshots() const;

private:
  WorldService() = default;

  mutable std::mutex m_worldMutex;
  MapRegistry m_mapRegistry;

  std::mutex m_auxMutex;
  std::shared_ptr<IGameScriptHost> m_scriptHost;
  std::shared_ptr<IMapCollisionQueries> m_collisionQueries;
  std::shared_ptr<ISpellCastTables const> m_spellCastTables;
  std::shared_ptr<ISpellDefinitionStore const> m_spellDefinitions;
  std::shared_ptr<SpellVisualDbc const> m_spellVisualDbc;
  std::shared_ptr<FactionTemplateDbc const> m_factionTemplateDbc;
  std::shared_ptr<PhaseGroupCatalog const> m_phaseGroupCatalog;
  std::shared_ptr<PhaseAreaCatalog const> m_phaseAreaCatalog;
  std::shared_ptr<AreaTableDbc const> m_areaTableDbc;
  std::shared_ptr<INpcTemplateSearchRepository const> m_npcTemplateSearch;
  ExperienceRates m_experienceRates{};
};

} // namespace Firelands
