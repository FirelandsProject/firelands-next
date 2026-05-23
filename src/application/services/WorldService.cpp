#include <application/services/WorldService.h>

#include <application/world/PhaseAreaCatalog.h>
#include <application/world/PhaseGroupCatalog.h>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/dbc/SpellVisualDbc.h>

namespace Firelands {

void WorldService::AddCreatureToMap(uint32 mapId,
                                    std::shared_ptr<Creature> creature) {
  const uint64 guid = creature->GetGuid();
  GetMap(mapId)->AddObject(std::move(creature));
  if (auto h = GetScriptHost()) {
    h->FireEvent("creature_spawn", guid);
  }
}

void WorldService::AddGameObjectToMap(uint32 mapId,
                                      std::shared_ptr<GameObject> object) {
  const uint64 guid = object->GetGuid();
  GetMap(mapId)->AddObject(std::move(object));
  if (auto h = GetScriptHost()) {
    h->FireEvent("gameobject_spawn", guid);
  }
}

void WorldService::SetScriptHost(std::shared_ptr<IGameScriptHost> host) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_scriptHost = std::move(host);
}

std::shared_ptr<IGameScriptHost> WorldService::GetScriptHost() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_scriptHost;
}

void WorldService::SetCollisionQueries(
    std::shared_ptr<IMapCollisionQueries> queries) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_collisionQueries = std::move(queries);
}

std::shared_ptr<IMapCollisionQueries> WorldService::GetCollisionQueries() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_collisionQueries;
}

void WorldService::SetSpellCastTables(std::shared_ptr<ISpellCastTables const> tables) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_spellCastTables = std::move(tables);
}

std::shared_ptr<ISpellCastTables const> WorldService::GetSpellCastTables() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_spellCastTables;
}

void WorldService::SetSpellDefinitions(
    std::shared_ptr<ISpellDefinitionStore const> definitions) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_spellDefinitions = std::move(definitions);
}

std::shared_ptr<ISpellDefinitionStore const> WorldService::GetSpellDefinitions() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_spellDefinitions;
}

void WorldService::SetSpellVisualDbc(std::shared_ptr<SpellVisualDbc const> spellVisualDbc) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_spellVisualDbc = std::move(spellVisualDbc);
}

std::shared_ptr<SpellVisualDbc const> WorldService::GetSpellVisualDbc() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_spellVisualDbc;
}

void WorldService::SetFactionTemplateDbc(
    std::shared_ptr<FactionTemplateDbc const> factionTemplateDbc) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_factionTemplateDbc = std::move(factionTemplateDbc);
}

std::shared_ptr<FactionTemplateDbc const> WorldService::GetFactionTemplateDbc() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_factionTemplateDbc;
}

void WorldService::SetPhaseGroupCatalog(
    std::shared_ptr<PhaseGroupCatalog const> catalog) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_phaseGroupCatalog = std::move(catalog);
}

std::shared_ptr<PhaseGroupCatalog const> WorldService::GetPhaseGroupCatalog() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_phaseGroupCatalog;
}

void WorldService::SetPhaseAreaCatalog(
    std::shared_ptr<PhaseAreaCatalog const> catalog) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_phaseAreaCatalog = std::move(catalog);
}

std::shared_ptr<PhaseAreaCatalog const> WorldService::GetPhaseAreaCatalog() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_phaseAreaCatalog;
}

void WorldService::SetExperienceRates(ExperienceRates rates) {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  m_experienceRates = rates;
}

ExperienceRates WorldService::GetExperienceRates() {
  std::lock_guard<std::mutex> lock(m_auxMutex);
  return m_experienceRates;
}

void WorldService::ForEachMap(
    std::function<void(uint32 mapId, std::shared_ptr<Map> const &)> const &fn) {
  std::lock_guard<std::mutex> lock(m_worldMutex);
  for (auto const &[mapId, map] : m_maps) {
    if (map)
      fn(mapId, map);
  }
}

void WorldService::ResetForShutdown() {
  std::unordered_map<uint32, std::shared_ptr<Map>> mapsToDestroy;
  {
    std::lock_guard<std::mutex> lock(m_worldMutex);
    mapsToDestroy.swap(m_maps);
  }
  {
    std::lock_guard<std::mutex> lock(m_auxMutex);
    m_scriptHost.reset();
    m_collisionQueries.reset();
    m_spellCastTables.reset();
    m_spellDefinitions.reset();
    m_spellVisualDbc.reset();
  }
  mapsToDestroy.clear();
}

} // namespace Firelands
