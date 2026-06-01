#pragma once

#include <application/logic/GossipLogic.h>
#include <application/logic/QuestProgressLogic.h>
#include <domain/models/QuestGiverStatus.h>
#include <domain/ports/IPlayerQuestProgress.h>
#include <domain/repositories/IQuestGossipRepository.h>
#include <shared/game/UnitNpcFlags.h>
#include <cstdint>
#include <vector>

namespace Firelands {

inline bool CreatureHasStarterQuests(IQuestGossipRepository const *repo,
                                     uint32_t creatureEntry, uint8_t playerClass,
                                     uint8_t playerRace, uint8_t playerLevel,
                                     IPlayerQuestProgress const &progress) {
  if (repo == nullptr || creatureEntry == 0)
    return false;
  auto const quests = repo->GetStarterQuestsForCreature(creatureEntry);
  for (auto const &summary : quests) {
    if (ResolveStarterQuestGossipIconForPlayer(summary, progress, playerClass, playerRace,
                                             playerLevel) != QuestGossipIcon::None)
      return true;
  }
  return false;
}

/// Aggregates per-quest dialog markers for this player (available / incomplete / reward).
inline QuestGiverDialogStatus
ResolveQuestGiverDialogStatus(IQuestGossipRepository const *repo,
                              uint32_t creatureEntry, uint8_t playerClass,
                              uint8_t playerRace, uint8_t playerLevel,
                              IPlayerQuestProgress const &progress) {
  return ResolveQuestGiverDialogStatusForPlayer(repo, creatureEntry, playerClass,
                                              playerRace, playerLevel, progress);
}

/// Wire `UNIT_NPC_FLAGS` for starter NPCs: keep template gossip (cata uses gossip menus for
/// quests); ensure quest giver; strip flight master so the client does not send taxi queries.
inline uint32_t EffectiveUnitNpcFlagsForCreature(uint32_t templateNpcFlags,
                                                 bool hasStarterQuests) {
  if (!hasStarterQuests)
    return templateNpcFlags;
  uint32_t flags = templateNpcFlags | kUnitNpcFlagQuestGiver;
  flags &= ~kUnitNpcFlagFlightMaster;
  return flags;
}

} // namespace Firelands
