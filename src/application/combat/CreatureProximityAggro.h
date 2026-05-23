#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Firelands {

class Creature;
class FactionTemplateDbc;
class Player;
struct FactionTemplateEntry;

} // namespace Firelands

namespace application {

/// Default horizontal detection for hostile NPCs (yards).
constexpr float kCreatureProximityAggroYards = 20.0f;

/// Player position for proximity scans (use live `WorldSession` coords when available).
struct ProximityAggroPlayer {
  uint64_t guid = 0;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  Firelands::Player const *player = nullptr;
};

/// True when this faction template should proactively attack players (monster / hates players).
bool FactionProvokesProximityAggro(Firelands::FactionTemplateEntry const &entry);

bool IsWithinProximityAggroRange(float creatureX, float creatureY, float creatureZ,
                                 float playerX, float playerY, float playerZ,
                                 float rangeYards = kCreatureProximityAggroYards);

/// Among `players`, returns the guid of the closest valid hostile target in aggro range.
std::optional<uint64_t> PickClosestHostilePlayerInAggroRange(
    Firelands::Creature const &creature, std::vector<ProximityAggroPlayer> const &players,
    Firelands::FactionTemplateDbc const *factionTemplates);

} // namespace application
