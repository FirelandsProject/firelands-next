#include <application/world/WorldRuntimeAccess.h>

#include <application/services/WorldService.h>
#include <application/world/PhaseAreaCatalog.h>
#include <application/world/PhaseGroupCatalog.h>
#include <domain/world/Creature.h>
#include <domain/world/GameObject.h>
#include <domain/world/Map.h>
#include <domain/world/Player.h>
#include <shared/dbc/AreaTableDbc.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/dbc/SpellVisualDbc.h>

namespace Firelands {

namespace {

class WorldServiceWorldRuntime final : public IWorldRuntime {
public:
  std::shared_ptr<Map> GetMap(uint32 mapId) override {
    return WorldService::Instance().GetMap(mapId);
  }

  void AddPlayerToMap(uint32 mapId, std::shared_ptr<Player> player) override {
    WorldService::Instance().AddPlayerToMap(mapId, std::move(player));
  }

  void RemovePlayerFromMap(uint32 mapId, uint64 guid) override {
    WorldService::Instance().RemovePlayerFromMap(mapId, guid);
  }

  void AddCreatureToMap(uint32 mapId,
                        std::shared_ptr<Creature> creature) override {
    WorldService::Instance().AddCreatureToMap(mapId, std::move(creature));
  }

  void AddGameObjectToMap(uint32 mapId,
                          std::shared_ptr<GameObject> object) override {
    WorldService::Instance().AddGameObjectToMap(mapId, std::move(object));
  }

  void ForEachMap(
      std::function<void(uint32 mapId, std::shared_ptr<Map> const &)> const
          &fn) override {
    WorldService::Instance().ForEachMap(fn);
  }

  std::shared_ptr<IGameScriptHost> GetScriptHost() override {
    return WorldService::Instance().GetScriptHost();
  }

  std::shared_ptr<IMapCollisionQueries> GetCollisionQueries() override {
    return WorldService::Instance().GetCollisionQueries();
  }

  std::shared_ptr<ISpellCastTables const> GetSpellCastTables() override {
    return WorldService::Instance().GetSpellCastTables();
  }

  std::shared_ptr<ISpellDefinitionStore const> GetSpellDefinitions() override {
    return WorldService::Instance().GetSpellDefinitions();
  }

  std::shared_ptr<SpellVisualDbc const> GetSpellVisualDbc() override {
    return WorldService::Instance().GetSpellVisualDbc();
  }

  std::shared_ptr<FactionTemplateDbc const> GetFactionTemplateDbc() override {
    return WorldService::Instance().GetFactionTemplateDbc();
  }

  std::shared_ptr<PhaseGroupCatalog const> GetPhaseGroupCatalog() override {
    return WorldService::Instance().GetPhaseGroupCatalog();
  }

  std::shared_ptr<PhaseAreaCatalog const> GetPhaseAreaCatalog() override {
    return WorldService::Instance().GetPhaseAreaCatalog();
  }

  std::shared_ptr<AreaTableDbc const> GetAreaTableDbc() override {
    return WorldService::Instance().GetAreaTableDbc();
  }

  ExperienceRates GetExperienceRates() override {
    return WorldService::Instance().GetExperienceRates();
  }
};

WorldServiceWorldRuntime &RuntimeSingleton() {
  static WorldServiceWorldRuntime instance;
  return instance;
}

} // namespace

IWorldRuntime &WorldRuntime() { return RuntimeSingleton(); }

std::shared_ptr<IWorldRuntime> WorldRuntimePtr() {
  static std::shared_ptr<IWorldRuntime> ptr =
      std::shared_ptr<IWorldRuntime>(&RuntimeSingleton(), [](IWorldRuntime *) {});
  return ptr;
}

} // namespace Firelands
