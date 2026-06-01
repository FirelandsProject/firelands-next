#pragma once

#include <domain/models/GossipMenu.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Firelands {

/// Quest line icon in `SMSG_GOSSIP_MESSAGE` / `SMSG_QUESTGIVER_QUEST_LIST`
/// (`PrepareQuestMenu` / `QUEST_ICON_*`). Not overhead `QuestGiverDialogStatus`.
enum class QuestGossipIcon : uint8_t {
  None = 0,
  Unavailable = 1,
  Available = 2, // yellow !
  Complete = 3, // yellow ?
  /// Wire 4 — ref `QuestMenu` ender lines (incomplete meet/kill); blue ? only when
  /// `blueQuestionMark` is set on the gossip quest line.
  CompleteDaily = 4,
};

/// Minimal quest fields required to render a gossip quest line.
struct QuestGossipSummary {
  uint32_t questId = 0;
  std::string title;
  /// `quest_template.QuestDescription` → `SMSG_QUESTGIVER_QUEST_DETAILS.LogDescription`.
  std::string questDescription;
  /// `quest_template.LogDescription` → `SMSG_QUESTGIVER_QUEST_DETAILS.Objectives`.
  std::string logDescription;
  /// `quest_template.OfferRewardText` → `SMSG_QUESTGIVER_OFFER_REWARD` (optional import).
  std::string offerRewardText;
  int32_t questLevel = 0;
  /// `quest_template.QuestSortID` — quest log zone header (e.g. 368 = Echo Isles).
  int32_t questSortId = 0;
  uint32_t flags = 0;
  uint32_t allowableClasses = 0;
  uint32_t allowableRaces = 0;
  /// `quest_template_addon.PrevQuestID` — positive: previous must be rewarded; negative: active.
  int32_t prevQuestId = 0;
  uint16_t soundAccept = 890;
  uint16_t soundTurnIn = 878;
  std::array<int32_t, 4> requiredNpcOrGo{};
  std::array<uint16_t, 4> requiredNpcOrGoCount{};
  std::array<uint32_t, 6> requiredItemId{};
  std::array<uint16_t, 6> requiredItemCount{};
  /// After `questFlags`: 0 = yellow ! styling, 1 = blue ? (autocomplete repeatables).
  bool blueQuestionMark = false;

  /// No kill/collect/speak objectives to track — ref completes via script or turn-in only.
  bool HasTrackableObjectives() const noexcept {
    for (size_t i = 0; i < requiredNpcOrGo.size(); ++i) {
      if (requiredNpcOrGo[i] != 0 && requiredNpcOrGoCount[i] != 0)
        return true;
    }
    for (size_t i = 0; i < requiredItemId.size(); ++i) {
      if (requiredItemId[i] != 0 && requiredItemCount[i] != 0)
        return true;
    }
    return false;
  }
};

/// `QUEST_FLAGS_AUTOCOMPLETE` (0x00010000).
inline constexpr uint32_t kQuestFlagAutoComplete = 0x00010000u;
/// `QUEST_FLAGS_AUTO_ACCEPT` (0x00080000) — ref adds quest on `CMSG_QUESTGIVER_QUERY_QUEST`.
inline constexpr uint32_t kQuestFlagAutoAccept = 0x00080000u;
inline constexpr uint32_t kQuestFlagDaily = 0x00001000u;
inline constexpr uint32_t kQuestFlagWeekly = 0x00008000u;

inline bool QuestHasAutoCompleteFlag(uint32_t flags) noexcept {
  return (flags & kQuestFlagAutoComplete) != 0;
}

inline bool QuestHasAutoAcceptFlag(uint32_t flags) noexcept {
  return (flags & kQuestFlagAutoAccept) != 0;
}

/// Wire flags for `SMSG_QUESTGIVER_QUEST_DETAILS` (ref masks out `AUTO_ACCEPT`).
inline uint32_t QuestFlagsForDetailsPacket(uint32_t flags) noexcept {
  return flags & ~kQuestFlagAutoAccept;
}

/// `GossipDef::SendGossipMenu` — not the same as `QUEST_FLAGS_AUTOCOMPLETE` alone.
inline bool QuestGossipUsesBlueQuestionMark(uint32_t flags) noexcept {
  if (!QuestHasAutoCompleteFlag(flags))
    return false;
  if ((flags & (kQuestFlagDaily | kQuestFlagWeekly)) != 0)
    return false;
  // Repeatable detection needs `SpecialFlags` (not in minimal import); default to yellow !.
    return false;
}

inline GossipQuestItem ToGossipQuestItem(QuestGossipSummary const &summary,
                                         QuestGossipIcon icon) {
  GossipQuestItem item;
  item.questId = summary.questId;
  item.questIcon = static_cast<uint8_t>(icon);
  item.questLevel = summary.questLevel;
  item.questFlags = summary.flags;
  item.blueQuestionMark = summary.blueQuestionMark;
  item.questTitle = summary.title;
  return item;
}

} // namespace Firelands
