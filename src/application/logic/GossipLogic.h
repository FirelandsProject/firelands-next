#pragma once

#include <domain/models/GossipMenu.h>
#include <domain/models/QuestGossip.h>
#include <shared/game/QuestMask.h>
#include <shared/game/UnitNpcFlags.h>
#include <cstdint>
#include <vector>

namespace Firelands {

/// Default gossip menu used when `creature_template.gossip_menu_id` is zero; options are
/// filtered by `OptionNpcflag` vs template `npcflag` (reference role shortcuts).
inline constexpr uint32_t kDefaultNpcGossipMenuId = 0u;

/// Picks the menu id to load from `gossip_menu` for a creature template.
inline uint32_t ResolveGossipMenuIdForTemplate(uint32_t templateGossipMenuId) noexcept {
  return templateGossipMenuId != 0 ? templateGossipMenuId : kDefaultNpcGossipMenuId;
}

/// True when the client expects `SMSG_GOSSIP_MESSAGE` (+ npc text), not quest list only.
/// Pure quest hubs (e.g. Jinthala 37951: `npcflag`=quest giver, `gossip_menu_id`=0) need
/// `SMSG_QUESTGIVER_QUEST_LIST` instead.
inline bool CreatureUsesGossipMenuDialog(uint32_t templateGossipMenuId,
                                         uint64_t templateNpcFlags) noexcept {
  if (templateGossipMenuId != 0)
    return true;
  return (templateNpcFlags & kUnitNpcFlagGossip) != 0;
}

/// Wire text id for `SMSG_NPC_TEXT_UPDATE` when `gossip_menu` has no row.
inline uint32_t ResolveGossipNpcTextId(uint32_t menuTextId) noexcept {
  return menuTextId != 0 ? menuTextId : 1u;
}

/// Keeps options with no npcflag gate or a matching role flag on the template.
inline std::vector<GossipMenuItem>
FilterGossipOptionsByNpcFlags(std::vector<GossipMenuItem> items,
                              uint64_t templateNpcFlags) {
  if (templateNpcFlags == 0)
    return items;
  std::vector<GossipMenuItem> filtered;
  filtered.reserve(items.size());
  for (auto &item : items) {
    if (item.optionNpcflag == 0 ||
        (item.optionNpcflag & templateNpcFlags) != 0)
      filtered.push_back(std::move(item));
}
  return filtered;
}

inline const GossipMenuItem *
FindGossipMenuItem(std::vector<GossipMenuItem> const &items,
                   uint32_t optionIndex) noexcept {
  for (auto const &item : items) {
    if (item.optionIndex == optionIndex)
      return &item;
}
  return nullptr;
}

/// True when `SMSG_GOSSIP_MESSAGE` has at least one option, text id, or quest line.
inline bool ShouldSendGossipMenu(size_t gossipOptionCount, bool hasMenuText,
                                 size_t questCount) noexcept {
  return questCount > 0 || gossipOptionCount > 0 || hasMenuText;
}

/// Until character quest status exists, starter quests show as available (yellow !).
inline QuestGossipIcon ResolveQuestGossipIcon(QuestGossipSummary const &) noexcept {
  return QuestGossipIcon::Available;
}

inline bool QuestGossipAllowsPlayer(QuestGossipSummary const &summary,
                                    uint8_t playerClass,
                                    uint8_t playerRace) noexcept {
  return QuestMaskAllowsPlayer(summary.allowableClasses,
                               PlayerClassMask(playerClass)) &&
         QuestMaskAllowsPlayer(summary.allowableRaces, PlayerRaceMask(playerRace));
}

inline std::vector<QuestGossipSummary>
FilterQuestGossipForPlayer(std::vector<QuestGossipSummary> quests,
                           uint8_t playerClass, uint8_t playerRace) {
  std::vector<QuestGossipSummary> filtered;
  filtered.reserve(quests.size());
  for (auto &summary : quests) {
    if (QuestGossipAllowsPlayer(summary, playerClass, playerRace))
      filtered.push_back(std::move(summary));
}
  return filtered;
}

inline std::vector<GossipQuestItem>
BuildGossipQuestItems(std::vector<QuestGossipSummary> const &quests) {
  std::vector<GossipQuestItem> items;
  items.reserve(quests.size());
  for (auto const &summary : quests) {
    QuestGossipSummary normalized = summary;
    normalized.blueQuestionMark = QuestGossipUsesBlueQuestionMark(summary.flags);
    items.push_back(
        ToGossipQuestItem(normalized, ResolveQuestGossipIcon(normalized)));
}
    return items;
}

} // namespace Firelands
