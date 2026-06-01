#pragma once

#include <application/logic/GossipLogic.h>
#include <domain/models/QuestGossip.h>
#include <domain/models/QuestGiverStatus.h>
#include <domain/models/QuestProgress.h>
#include <domain/ports/IPlayerQuestProgress.h>
#include <domain/repositories/IQuestGossipRepository.h>
#include <cstdint>
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
  PrerequisitesNotMet,
};

inline std::optional<QuestGossipSummary>
FindStarterQuestForCreature(IQuestGossipRepository const *repo, uint32_t creatureEntry,
                            uint32_t questId) {
  if (!repo || creatureEntry == 0 || questId == 0)
    return std::nullopt;
  return repo->TryGetStarterQuestForCreature(creatureEntry, questId);
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

/// Ref `Player::SatisfyQuestPreviousQuest` (positive prev = rewarded, negative = active).
inline bool SatisfyQuestPreviousQuest(int32_t prevQuestId,
                                      IPlayerQuestProgress const &progress) {
  if (prevQuestId == 0)
    return true;
  uint32_t const prevId = prevQuestId > 0 ? static_cast<uint32_t>(prevQuestId)
                                        : static_cast<uint32_t>(-prevQuestId);
  if (prevQuestId > 0)
    return progress.IsQuestRewarded(prevId);
  return progress.GetQuestStatus(prevId) == QuestStatus::Incomplete;
}

/// Ref `Player::SatisfyQuestLevel` — `quest_template.QuestLevel` is minimum level to accept.
inline bool SatisfyQuestMinLevel(QuestGossipSummary const &summary,
                                 uint8_t playerLevel) noexcept {
  if (summary.questLevel <= 0)
    return true;
  return static_cast<int32_t>(playerLevel) >= summary.questLevel;
}

inline bool PlayerMayTakeStarterQuest(QuestGossipSummary const &summary,
                                      IPlayerQuestProgress const &progress,
                                      uint8_t playerClass, uint8_t playerRace,
                                      uint8_t playerLevel) {
  if (!QuestGossipAllowsPlayer(summary, playerClass, playerRace))
    return false;
  if (progress.IsQuestRewarded(summary.questId))
    return false;
  if (progress.GetQuestStatus(summary.questId) != QuestStatus::None)
    return false;
  if (!SatisfyQuestPreviousQuest(summary.prevQuestId, progress))
    return false;
  if (!SatisfyQuestMinLevel(summary, playerLevel))
    return false;
  return true;
}

/// Meet/turn-in at end NPC (ref: no kill/item counters; often `RequiredNpcOrGo` all zero).
inline bool QuestCompletesOnMeetNpc(QuestGossipSummary const &summary,
                                    uint32_t endCreatureEntry) noexcept {
  if (QuestHasAutoCompleteFlag(summary.flags) || endCreatureEntry == 0)
    return false;
  if (summary.HasTrackableObjectives())
    return false;
  for (size_t i = 0; i < summary.requiredNpcOrGo.size(); ++i) {
    if (summary.requiredNpcOrGo[i] == 0 || summary.requiredNpcOrGoCount[i] == 0)
      continue;
    if (summary.requiredNpcOrGo[i] > 0 &&
        static_cast<uint32_t>(summary.requiredNpcOrGo[i]) == endCreatureEntry)
      return true;
    return false;
  }
  return true;
}

inline QuestAcceptResult EvaluateQuestAccept(QuestGossipSummary const &summary,
                                             IPlayerQuestProgress const &progress,
                                             uint8_t playerClass, uint8_t playerRace,
                                             uint8_t playerLevel) {
  if (!QuestGossipAllowsPlayer(summary, playerClass, playerRace))
    return QuestAcceptResult::ClassRaceNotAllowed;
  if (progress.IsQuestRewarded(summary.questId))
    return QuestAcceptResult::AlreadyRewarded;
  QuestStatus const status = progress.GetQuestStatus(summary.questId);
  if (status == QuestStatus::Incomplete || status == QuestStatus::Complete)
    return QuestAcceptResult::AlreadyOnQuest;
  if (!SatisfyQuestPreviousQuest(summary.prevQuestId, progress))
    return QuestAcceptResult::PrerequisitesNotMet;
  if (!SatisfyQuestMinLevel(summary, playerLevel))
    return QuestAcceptResult::PrerequisitesNotMet;
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
  case QuestAcceptResult::PrerequisitesNotMet:
    return 0u; // INVALIDREASON_DONT_HAVE_REQ
  default:
    return 0u;
  }
}

inline QuestGossipIcon ResolveStarterQuestGossipIconForPlayer(
    QuestGossipSummary const &summary, IPlayerQuestProgress const &progress,
    uint8_t playerClass, uint8_t playerRace, uint8_t playerLevel) {
  if (progress.IsQuestRewarded(summary.questId))
    return QuestGossipIcon::Unavailable;
  switch (progress.GetQuestStatus(summary.questId)) {
  case QuestStatus::Complete:
    return QuestGossipIcon::None;
  case QuestStatus::Incomplete:
    return QuestGossipIcon::None;
  default:
    if (!QuestGossipAllowsPlayer(summary, playerClass, playerRace))
      return QuestGossipIcon::None;
    if (PlayerMayTakeStarterQuest(summary, progress, playerClass, playerRace,
                                playerLevel))
      return QuestGossipIcon::Available;
    // Ref `PrepareQuestMenu`: chain/class gates hide the line; level alone shows grey.
    if (!SatisfyQuestPreviousQuest(summary.prevQuestId, progress))
      return QuestGossipIcon::None;
    if (!SatisfyQuestMinLevel(summary, playerLevel))
      return QuestGossipIcon::Unavailable;
    return QuestGossipIcon::None;
  }
}

/// Turn-in dialog body when `OfferRewardText` is not loaded from DB.
inline std::string QuestOfferRewardTextForPlayer(QuestGossipSummary const &summary) {
  if (!summary.offerRewardText.empty())
    return summary.offerRewardText;
  if (!summary.questDescription.empty())
    return summary.questDescription;
  return summary.logDescription;
}

inline QuestGossipIcon
ResolveEnderQuestGossipIconForPlayer(QuestGossipSummary const &summary,
                                   IPlayerQuestProgress const &progress) {
  if (progress.IsQuestRewarded(summary.questId))
    return QuestGossipIcon::Unavailable;
  switch (progress.GetQuestStatus(summary.questId)) {
  case QuestStatus::Complete:
  case QuestStatus::Incomplete:
    // Ref `PrepareQuestMenu` — ender lines always use icon 4 (meet/kill/turn-in).
    return QuestGossipIcon::CompleteDaily;
  default:
    return QuestGossipIcon::None;
  }
}

inline QuestGiverDialogStatus
ResolveStarterPerQuestDialogStatus(QuestGossipSummary const &summary,
                                   IPlayerQuestProgress const &progress,
                                   uint8_t playerClass, uint8_t playerRace,
                                   uint8_t playerLevel) {
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
    return PlayerMayTakeStarterQuest(summary, progress, playerClass, playerRace,
                                   playerLevel)
               ? QuestGiverDialogStatus::Available
               : QuestGiverDialogStatus::None;
  }
}

/// Client overhead marker: turn-in beats starter ! when both apply (raw enum order is wrong).
inline uint32_t QuestGiverDialogStatusDisplayPriority(
    QuestGiverDialogStatus status) noexcept {
  switch (status) {
  case QuestGiverDialogStatus::Reward:
  case QuestGiverDialogStatus::Reward2:
    return 100u;
  case QuestGiverDialogStatus::Incomplete:
    return 80u;
  case QuestGiverDialogStatus::Available:
    return 60u;
  case QuestGiverDialogStatus::AvailableRep:
    return 55u;
  case QuestGiverDialogStatus::RewardRep:
    return 50u;
  case QuestGiverDialogStatus::LowLevelAvailable:
  case QuestGiverDialogStatus::LowLevelAvailableRep:
    return 40u;
  case QuestGiverDialogStatus::LowLevelRewardRep:
    return 35u;
  case QuestGiverDialogStatus::Unavailable:
    return 10u;
  default:
    return 0u;
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
  switch (progress.GetQuestStatus(summary.questId)) {
  case QuestStatus::Complete:
    return QuestGiverDialogStatus::Reward;
  case QuestStatus::Incomplete:
    return QuestGiverDialogStatus::Incomplete;
  default:
    return QuestGiverDialogStatus::None;
  }
}

inline QuestGiverDialogStatus ResolveQuestGiverDialogStatusForPlayer(
    IQuestGossipRepository const *repo, uint32_t creatureEntry, uint8_t playerClass,
    uint8_t playerRace, uint8_t playerLevel,
    IPlayerQuestProgress const &progress) {
  if (!repo || creatureEntry == 0)
    return QuestGiverDialogStatus::None;

  QuestGiverDialogStatus best = QuestGiverDialogStatus::None;
  uint32_t bestPriority = 0u;
  auto const consider = [&](QuestGiverDialogStatus st) {
    uint32_t const priority = QuestGiverDialogStatusDisplayPriority(st);
    if (priority > bestPriority) {
      bestPriority = priority;
      best = st;
    }
  };
  for (QuestGossipSummary const &summary :
       repo->GetStarterQuestsForCreature(creatureEntry)) {
    consider(ResolveStarterPerQuestDialogStatus(summary, progress, playerClass,
                                              playerRace, playerLevel));
  }
  for (QuestGossipSummary const &summary :
       repo->GetEnderQuestsForCreature(creatureEntry)) {
    consider(ResolveEnderPerQuestDialogStatus(summary, progress, playerClass,
                                              playerRace));
  }
  return best;
}

inline std::vector<GossipQuestItem> BuildStarterGossipQuestItemsForPlayer(
    std::vector<QuestGossipSummary> const &quests, IPlayerQuestProgress const &progress,
    uint8_t playerClass, uint8_t playerRace, uint8_t playerLevel) {
  std::vector<GossipQuestItem> items;
  items.reserve(quests.size());
  for (auto const &summary : quests) {
    QuestGossipIcon const icon = ResolveStarterQuestGossipIconForPlayer(
        summary, progress, playerClass, playerRace, playerLevel);
    if (icon == QuestGossipIcon::None)
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
    uint8_t playerClass, uint8_t playerRace, uint8_t playerLevel,
    IPlayerQuestProgress const &progress) {
  if (!repo || creatureEntry == 0)
    return {};
  auto starters = FilterQuestGossipForPlayer(
      repo->GetStarterQuestsForCreature(creatureEntry), playerClass, playerRace);
  auto enders = FilterQuestGossipForPlayer(
      repo->GetEnderQuestsForCreature(creatureEntry), playerClass, playerRace);
  auto items = BuildStarterGossipQuestItemsForPlayer(starters, progress, playerClass,
                                                     playerRace, playerLevel);
  auto const enderItems = BuildEnderGossipQuestItemsForPlayer(enders, progress);
  items.insert(items.end(), enderItems.begin(), enderItems.end());
  return items;
}

} // namespace Firelands
