#pragma once

#include <domain/models/GossipMenu.h>
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
  CompleteDaily = 4, // blue ?
};

/// Minimal quest fields required to render a gossip quest line.
struct QuestGossipSummary {
  uint32_t questId = 0;
  std::string title;
  int32_t questLevel = 0;
  uint32_t flags = 0;
  uint32_t allowableClasses = 0;
  uint32_t allowableRaces = 0;
  /// After `questFlags`: 0 = yellow ! styling, 1 = blue ? (autocomplete repeatables).
  bool blueQuestionMark = false;
};

/// `QUEST_FLAGS_AUTOCOMPLETE` (0x00010000).
inline constexpr uint32_t kQuestFlagAutoComplete = 0x00010000u;
inline constexpr uint32_t kQuestFlagDaily = 0x00001000u;
inline constexpr uint32_t kQuestFlagWeekly = 0x00008000u;

inline bool QuestHasAutoCompleteFlag(uint32_t flags) noexcept {
  return (flags & kQuestFlagAutoComplete) != 0;
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
