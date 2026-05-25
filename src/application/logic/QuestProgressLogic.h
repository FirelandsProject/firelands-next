#pragma once

#include <application/logic/GossipLogic.h>
#include <domain/models/QuestGossip.h>
#include <domain/models/QuestGiverStatus.h>
#include <domain/models/QuestProgress.h>
#include <domain/ports/IPlayerQuestProgress.h>
#include <domain/repositories/IQuestGossipRepository.h>
#include <optional>
#include <vector>

namespace Firelands {

enum class QuestAcceptResult : uint8_t {
  Accepted,
  InvalidQuest,
  NotStarterForNpc,
  ClassRaceNotAllowed,
  AlreadyOnQuest,
  AlreadyRewarded,
};

inline std::optional<QuestGossipSummary>
FindStarterQuestForCreature(IQuestGossipRepository const *repo, uint32_t creatureEntry,
                            uint32_t questId) {
  if (!repo || creatureEntry == 0 || questId == 0)
    return std::nullopt;
  for (QuestGossipSummary const &summary :
       repo->GetStarterQuestsForCreature(creatureEntry)) {
    if (summary.questId == questId)
      return summary;
  }
  return std::nullopt;
}

inline std::optional<QuestGossipSummary>
FindEnderQuestForCreature(IQuestGossipRepository const *repo, uint32_t creatureEntry,
                          uint32_t questId) {
  if (!repo || creatureEntry == 0 || questId == 0)
    return std::nullopt;
  for (QuestGossipSummary const &summary :
       repo->GetEnderQuestsForCreature(creatureEntry)) {
    if (summary.questId == questId)
      return summary;
  }
  return std::nullopt;
}

inline bool PlayerMayTakeStarterQuest(QuestGossipSummary const &summary,
                                      IPlayerQuestProgress const &progress,
                                      uint8_t playerClass, uint8_t playerRace) {
  if (!QuestGossipAllowsPlayer(summary, playerClass, playerRace))
    return false;
  if (progress.IsQuestRewarded(summary.questId))
    return false;
  if (progress.GetQuestStatus(summary.questId) != QuestStatus::None)
    return false;
  return true;
}

/// Meet-NPC quests (no kill/collect objectives) complete when the player opens the end NPC.
inline bool QuestCompletesOnMeetNpc(QuestGossipSummary const &summary) noexcept {
  return !QuestHasAutoCompleteFlag(summary.flags) && !summary.HasTrackableObjectives();
}

inline QuestAcceptResult EvaluateQuestAccept(QuestGossipSummary const &summary,
                                             IPlayerQuestProgress const &progress,
                                             uint8_t playerClass, uint8_t playerRace) {
  if (!QuestGossipAllowsPlayer(summary, playerClass, playerRace))
    return QuestAcceptResult::ClassRaceNotAllowed;
  if (progress.IsQuestRewarded(summary.questId))
    return QuestAcceptResult::AlreadyRewarded;
  QuestStatus const status = progress.GetQuestStatus(summary.questId);
  if (status == QuestStatus::Incomplete || status == QuestStatus::Complete)
    return QuestAcceptResult::AlreadyOnQuest;
  return QuestAcceptResult::Accepted;
}

/// Maps accept evaluation to `SMSG_QUESTGIVER_INVALID_QUEST` reason (Cataclysm ref).
inline uint32_t QuestAcceptResultToInvalidReason(QuestAcceptResult result) {
  switch (result) {
  case QuestAcceptResult::AlreadyOnQuest:
    return 13u; // INVALIDREASON_QUEST_ALREADY_ON
  case QuestAcceptResult::AlreadyRewarded:
    return 7u; // INVALIDREASON_QUEST_ALREADY_DONE
  case QuestAcceptResult::ClassRaceNotAllowed:
    return 6u; // INVALIDREASON_QUEST_FAILED_WRONG_RACE
  default:
    return 0u;
  }
}

inline QuestGossipIcon
ResolveStarterQuestGossipIconForPlayer(QuestGossipSummary const &summary,
                                       IPlayerQuestProgress const &progress) {
  if (progress.IsQuestRewarded(summary.questId))
    return QuestGossipIcon::Unavailable;
  switch (progress.GetQuestStatus(summary.questId)) {
  case QuestStatus::Complete:
    return QuestGossipIcon::None;
  case QuestStatus::Incomplete:
    return QuestGossipIcon::None;
  default:
    return QuestGossipIcon::Available;
  }
}

inline QuestGossipIcon
ResolveEnderQuestGossipIconForPlayer(QuestGossipSummary const &summary,
                                   IPlayerQuestProgress const &progress) {
  if (progress.IsQuestRewarded(summary.questId))
    return QuestGossipIcon::Unavailable;
  if (progress.GetQuestStatus(summary.questId) == QuestStatus::Complete)
    return QuestGossipIcon::Complete;
  return QuestGossipIcon::None;
}

inline QuestGiverDialogStatus
ResolveStarterPerQuestDialogStatus(QuestGossipSummary const &summary,
                                   IPlayerQuestProgress const &progress,
                                   uint8_t playerClass, uint8_t playerRace) {
  if (!QuestGossipAllowsPlayer(summary, playerClass, playerRace))
    return QuestGiverDialogStatus::None;
  if (progress.IsQuestRewarded(summary.questId))
    return QuestGiverDialogStatus::None;
  switch (progress.GetQuestStatus(summary.questId)) {
  case QuestStatus::Complete:
    return QuestGiverDialogStatus::None;
  case QuestStatus::Incomplete:
    return QuestGiverDialogStatus::Incomplete;
  default:
    return QuestGiverDialogStatus::Available;
  }
}

inline QuestGiverDialogStatus
ResolveEnderPerQuestDialogStatus(QuestGossipSummary const &summary,
                               IPlayerQuestProgress const &progress,
                               uint8_t playerClass, uint8_t playerRace) {
  if (!QuestGossipAllowsPlayer(summary, playerClass, playerRace))
    return QuestGiverDialogStatus::None;
  if (progress.IsQuestRewarded(summary.questId))
    return QuestGiverDialogStatus::None;
  if (progress.GetQuestStatus(summary.questId) == QuestStatus::Complete)
    return QuestGiverDialogStatus::Reward;
  return QuestGiverDialogStatus::None;
}

inline QuestGiverDialogStatus ResolveQuestGiverDialogStatusForPlayer(
    IQuestGossipRepository const *repo, uint32_t creatureEntry, uint8_t playerClass,
    uint8_t playerRace, IPlayerQuestProgress const &progress) {
  if (!repo || creatureEntry == 0)
    return QuestGiverDialogStatus::None;

  QuestGiverDialogStatus best = QuestGiverDialogStatus::None;
  for (QuestGossipSummary const &summary :
       repo->GetStarterQuestsForCreature(creatureEntry)) {
    QuestGiverDialogStatus const st =
        ResolveStarterPerQuestDialogStatus(summary, progress, playerClass, playerRace);
    if (static_cast<uint32_t>(st) > static_cast<uint32_t>(best))
      best = st;
  }
  for (QuestGossipSummary const &summary :
       repo->GetEnderQuestsForCreature(creatureEntry)) {
    QuestGiverDialogStatus const st =
        ResolveEnderPerQuestDialogStatus(summary, progress, playerClass, playerRace);
    if (static_cast<uint32_t>(st) > static_cast<uint32_t>(best))
      best = st;
  }
  return best;
}

inline std::vector<GossipQuestItem>
BuildStarterGossipQuestItemsForPlayer(std::vector<QuestGossipSummary> const &quests,
                                      IPlayerQuestProgress const &progress) {
  std::vector<GossipQuestItem> items;
  items.reserve(quests.size());
  for (auto const &summary : quests) {
    QuestGossipIcon const icon =
        ResolveStarterQuestGossipIconForPlayer(summary, progress);
    if (icon == QuestGossipIcon::None || icon == QuestGossipIcon::Unavailable)
      continue;
    QuestGossipSummary normalized = summary;
    normalized.blueQuestionMark = QuestGossipUsesBlueQuestionMark(summary.flags);
    items.push_back(ToGossipQuestItem(normalized, icon));
  }
  return items;
}

inline std::vector<GossipQuestItem>
BuildEnderGossipQuestItemsForPlayer(std::vector<QuestGossipSummary> const &quests,
                                    IPlayerQuestProgress const &progress) {
  std::vector<GossipQuestItem> items;
  items.reserve(quests.size());
  for (auto const &summary : quests) {
    QuestGossipIcon const icon = ResolveEnderQuestGossipIconForPlayer(summary, progress);
    if (icon == QuestGossipIcon::None || icon == QuestGossipIcon::Unavailable)
      continue;
    QuestGossipSummary normalized = summary;
    normalized.blueQuestionMark = QuestGossipUsesBlueQuestionMark(summary.flags);
    items.push_back(ToGossipQuestItem(normalized, icon));
  }
  return items;
}

inline std::vector<GossipQuestItem> BuildAllGossipQuestItemsForPlayer(
    IQuestGossipRepository const *repo, uint32_t creatureEntry,
    uint8_t playerClass, uint8_t playerRace, IPlayerQuestProgress const &progress) {
  if (!repo || creatureEntry == 0)
    return {};
  auto starters = FilterQuestGossipForPlayer(
      repo->GetStarterQuestsForCreature(creatureEntry), playerClass, playerRace);
  auto enders = FilterQuestGossipForPlayer(
      repo->GetEnderQuestsForCreature(creatureEntry), playerClass, playerRace);
  auto items = BuildStarterGossipQuestItemsForPlayer(starters, progress);
  auto const enderItems = BuildEnderGossipQuestItemsForPlayer(enders, progress);
  items.insert(items.end(), enderItems.begin(), enderItems.end());
  return items;
}

} // namespace Firelands
