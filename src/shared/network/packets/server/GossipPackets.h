#pragma once

#include <domain/models/GossipMenu.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Firelands::gossip {

/// Cataclysm 4.3.4 gossip wire helpers.
/// Reference: `PlayerMenu::SendGossipMenu` / TCPP `WorldPackets::NPC::GossipMessage`.
inline void AppendGossipMenuItem(WorldPacket &out, GossipMenuItem const &item) {
  out.Append<int32_t>(static_cast<int32_t>(item.optionIndex));
  out.Append<uint8_t>(static_cast<uint8_t>(item.icon));
  out.Append<int8_t>(item.isCoded ? 1 : 0);
  out.Append<int32_t>(static_cast<int32_t>(item.boxMoney));
  out.WriteString(item.optionText);
  out.WriteString(item.boxMessage);
}

/// 3.3.5+ quest line in `SMSG_GOSSIP_MESSAGE` (quest_id, icon, level, flags, blue?, title).
inline void AppendGossipQuestItem(WorldPacket &out, GossipQuestItem const &quest) {
  out.Append<int32_t>(static_cast<int32_t>(quest.questId));
  out.Append<int32_t>(static_cast<int32_t>(quest.questIcon));
  out.Append<int32_t>(quest.questLevel);
  out.Append<int32_t>(static_cast<int32_t>(quest.questFlags));
  out.Append<uint8_t>(quest.blueQuestionMark ? 1u : 0u);
  out.WriteString(quest.questTitle);
}

/// Build `SMSG_GOSSIP_MESSAGE` — gossip options and optional quest lines.
inline WorldPacket BuildGossipMessage(uint64_t npcGuid, uint32_t menuId, uint32_t textId,
                                      std::vector<GossipMenuItem> const &items,
                                      std::vector<GossipQuestItem> const &quests = {}) {
  WorldPacket out(SMSG_GOSSIP_MESSAGE, 100);
  out.Append<uint64_t>(npcGuid);
  out.Append<int32_t>(static_cast<int32_t>(menuId));
  out.Append<int32_t>(static_cast<int32_t>(textId));
  out.Append<uint32_t>(static_cast<uint32_t>(items.size()));
  for (auto const &item : items)
    AppendGossipMenuItem(out, item);

  out.Append<uint32_t>(static_cast<uint32_t>(quests.size()));
  for (auto const &quest : quests)
    AppendGossipQuestItem(out, quest);
  return out;
}

/// Build `SMSG_GOSSIP_COMPLETE` — closes the gossip window.
inline WorldPacket BuildGossipComplete() {
  WorldPacket out(SMSG_GOSSIP_COMPLETE, 0);
  return out;
}

/// Build `SMSG_GOSSIP_MESSAGE` with a single text-only greeting.
inline WorldPacket BuildGossipMessageSimple(uint64_t npcGuid, uint32_t menuId,
                                            uint32_t textId) {
  std::vector<GossipMenuItem> empty;
  return BuildGossipMessage(npcGuid, menuId, textId, empty);
}

} // namespace Firelands::gossip
