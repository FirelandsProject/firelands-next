#pragma once

#include <domain/models/GossipMenu.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Firelands::quest {

struct QuestGiverStatusEntry {
  uint64_t guid = 0;
  uint32_t status = 0;
};

/// `SMSG_QUESTGIVER_STATUS` — Cataclysm 4.3.4: uint64 ObjectGuid + uint32 status flags
/// (matches `GossipDef` / client `CMSG_QUESTGIVER_STATUS_QUERY` with 8-byte guid).
inline WorldPacket BuildQuestGiverStatus(uint64_t npcGuid, uint32_t dialogStatus) {
  WorldPacket out(SMSG_QUESTGIVER_STATUS, 16);
  out.Append<uint64_t>(npcGuid);
  out.Append<uint32_t>(dialogStatus);
  return out;
}

/// `SMSG_QUESTGIVER_STATUS_MULTIPLE` — int32 count, then (uint64 guid + uint32 status)*.
inline WorldPacket
BuildQuestGiverStatusMultiple(std::vector<QuestGiverStatusEntry> const &entries) {
  WorldPacket out(SMSG_QUESTGIVER_STATUS_MULTIPLE, 8 + entries.size() * 12);
  out.Append<int32_t>(static_cast<int32_t>(entries.size()));
  for (auto const &entry : entries) {
    out.Append<uint64_t>(entry.guid);
    out.Append<uint32_t>(entry.status);
  }
  return out;
}

/// `SMSG_QUESTGIVER_QUEST_LIST` — pure quest giver dialog (4.3.4: uint64 guid + quest lines).
inline WorldPacket BuildQuestGiverQuestListMessage(
    uint64_t npcGuid, std::string const &greeting,
    std::vector<GossipQuestItem> const &quests) {
  WorldPacket out(SMSG_QUESTGIVER_QUEST_LIST, 128);
  out.Append<uint64_t>(npcGuid);
  out.WriteString(greeting);
  out.Append<uint32_t>(0); // greet emote delay
  out.Append<uint32_t>(0); // greet emote type
  out.Append<uint8_t>(static_cast<uint8_t>(quests.size()));
  for (auto const &quest : quests) {
    out.Append<int32_t>(static_cast<int32_t>(quest.questId));
    out.Append<int32_t>(static_cast<int32_t>(quest.questIcon));
    out.Append<int32_t>(quest.questLevel);
    out.Append<int32_t>(static_cast<int32_t>(quest.questFlags));
    out.Append<uint8_t>(quest.blueQuestionMark ? 1u : 0u);
    out.WriteString(quest.questTitle);
  }
  return out;
}

/// `SMSG_QUESTGIVER_INVALID_QUEST` — quest not available (minimal; unblocks client UI).
inline WorldPacket BuildQuestGiverInvalidQuest() {
  WorldPacket out(SMSG_QUESTGIVER_INVALID_QUEST, 0);
  return out;
}

} // namespace Firelands::quest
