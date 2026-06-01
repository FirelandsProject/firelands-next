#pragma once

#include <domain/models/GossipMenu.h>
#include <domain/models/QuestGossip.h>
#include <shared/network/BitWriter.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Firelands::quest {

/// Cataclysm 4.3.4 quest reward block sizes (`QuestDef.h` in firelands-cata-ref).
inline constexpr uint32_t kQuestRewardChoicesCount = 6u;
inline constexpr uint32_t kQuestRewardsCount = 4u;
inline constexpr uint32_t kQuestReputationsCount = 5u;
inline constexpr uint32_t kQuestRewardCurrencyCount = 4u;
inline constexpr uint32_t kQuestRequiredCurrencyCount = 4u;
inline constexpr uint32_t kQuestObjectivesCount = 4u;
inline constexpr uint32_t kQuestItemObjectivesCount = 6u;
inline constexpr uint32_t kQuestDescEmoteCount = 4u;
inline constexpr uint32_t kBgTeamsCount = 2u;
inline constexpr uint32_t kQuestFlagsHiddenRewards = 0x00000200u;

inline int32_t QuestQueryWireType(uint32_t flags) noexcept {
  return QuestHasAutoCompleteFlag(flags) ? 0 : 2;
}

/// Zeroed `QuestRewards` block for `SMSG_QUESTGIVER_QUEST_DETAILS` (4.3.4 wire order).
inline void AppendQuestRewardsBlock(WorldPacket &out) {
  out.Append<uint32_t>(0u); // ChoiceItemCount
  for (uint32_t i = 0; i < kQuestRewardChoicesCount; ++i)
    out.Append<uint32_t>(0u); // choice ItemID
  for (uint32_t i = 0; i < kQuestRewardChoicesCount; ++i)
    out.Append<uint32_t>(0u); // choice Quantity
  for (uint32_t i = 0; i < kQuestRewardChoicesCount; ++i)
    out.Append<uint32_t>(0u); // choice DisplayID

  out.Append<uint32_t>(0u); // ItemCount
  for (uint32_t i = 0; i < kQuestRewardsCount; ++i)
    out.Append<uint32_t>(0u);
  for (uint32_t i = 0; i < kQuestRewardsCount; ++i)
    out.Append<uint32_t>(0u);
  for (uint32_t i = 0; i < kQuestRewardsCount; ++i)
    out.Append<uint32_t>(0u);

  out.Append<uint32_t>(0u); // Money
  out.Append<uint32_t>(0u); // XP
  out.Append<uint32_t>(0u); // Title
  out.Append<uint32_t>(0u); // unknown
  out.Append<float>(0.f);   // unknown
  out.Append<uint32_t>(0u); // NumBonusTalents
  out.Append<uint32_t>(0u); // unknown
  out.Append<uint32_t>(0u); // FactionFlags

  for (uint32_t i = 0; i < kQuestReputationsCount; ++i)
    out.Append<uint32_t>(0u);
  for (uint32_t i = 0; i < kQuestReputationsCount; ++i)
    out.Append<int32_t>(0);
  for (uint32_t i = 0; i < kQuestReputationsCount; ++i)
    out.Append<uint32_t>(0u);

  out.Append<uint32_t>(0u); // SpellCompletionDisplayID
  out.Append<uint32_t>(0u); // SpellCompletionID

  for (uint32_t i = 0; i < kQuestRewardCurrencyCount; ++i)
    out.Append<uint32_t>(0u);
  for (uint32_t i = 0; i < kQuestRewardCurrencyCount; ++i)
    out.Append<uint32_t>(0u);

  out.Append<uint32_t>(0u); // SkillLineID
  out.Append<uint32_t>(0u); // NumSkillUps
}

/// `SMSG_QUESTGIVER_QUEST_DETAILS` (4.3.4) — matches `QuestGiverQuestDetails::Write` in ref.
inline WorldPacket BuildQuestGiverQuestDetails(uint64_t npcGuid, uint32_t questId,
                                               QuestGossipSummary const &summary) {
  WorldPacket out(SMSG_QUESTGIVER_QUEST_DETAILS, 512);
  out.Append<uint64_t>(npcGuid);
  out.Append<uint64_t>(0u); // InformUnit
  out.Append<uint32_t>(questId);
  out.WriteString(summary.title);
  out.WriteString(summary.questDescription);
  out.WriteString(summary.logDescription);
  out.WriteString("");
  out.WriteString("");
  out.WriteString("");
  out.WriteString("");
  out.Append<uint32_t>(0u); // PortraitGiver
  out.Append<uint32_t>(0u); // PortraitTurnIn
  out.Append<uint8_t>(1u);  // AutoLaunched
  out.Append<uint32_t>(QuestFlagsForDetailsPacket(summary.flags));
  out.Append<uint32_t>(0u); // SuggestedPartyMembers
  out.Append<uint8_t>(0u);  // StartCheat
  out.Append<uint8_t>(0u);  // DisplayPopup
  out.Append<uint32_t>(0u); // RequiredSpellID
  AppendQuestRewardsBlock(out);
  out.Append<uint32_t>(kQuestDescEmoteCount);
  for (uint32_t i = 0; i < kQuestDescEmoteCount; ++i) {
    out.Append<uint32_t>(0u); // Type
    out.Append<uint32_t>(0u); // Delay
  }
  return out;
}

/// `SMSG_QUESTGIVER_OFFER_REWARD` (4.3.4) — ref `QuestGiverOfferRewardMessage::Write`.
inline WorldPacket BuildQuestGiverOfferReward(uint64_t npcGuid, uint32_t questId,
                                              QuestGossipSummary const &summary,
                                              std::string const &rewardText) {
  WorldPacket out(SMSG_QUESTGIVER_OFFER_REWARD, 512);
  out.Append<uint64_t>(npcGuid);
  out.Append<uint32_t>(questId);
  out.WriteString(summary.title);
  out.WriteString(rewardText);
  out.WriteString("");
  out.WriteString("");
  out.WriteString("");
  out.WriteString("");
  out.Append<uint32_t>(0u); // PortraitGiver
  out.Append<uint32_t>(0u); // PortraitTurnIn
  out.Append<uint8_t>(1u);  // AutoLaunched
  out.Append<uint32_t>(summary.flags);
  out.Append<uint32_t>(0u); // SuggestedPartyMembers
  out.Append<uint32_t>(0u); // Emote count
  AppendQuestRewardsBlock(out);
  return out;
}

inline WorldPacket BuildQuestUpdateComplete(uint32_t questId) {
  WorldPacket out(SMSG_QUEST_UPDATE_COMPLETE, 8);
  out.Append<uint32_t>(questId);
  return out;
}

/// Autocomplete finish (`SMSG_QUESTGIVER_QUEST_COMPLETE`, 4.3.4 bit tail).
inline WorldPacket BuildQuestGiverQuestComplete(uint32_t questId) {
  WorldPacket out(SMSG_QUESTGIVER_QUEST_COMPLETE, 32);
  out.Append<uint32_t>(0u); // TalentReward
  out.Append<uint32_t>(0u); // NumSkillUpsReward
  out.Append<uint32_t>(0u); // MoneyReward
  out.Append<uint32_t>(0u); // XPReward
  out.Append<uint32_t>(questId);
  out.Append<uint32_t>(0u); // SkillLineIDReward
  BitWriter bw(out);
  bw.WriteBit(false); // LaunchGossip
  bw.WriteBit(false); // UseQuestReward
  bw.Flush();
  return out;
}

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
    out.Append<uint32_t>(quest.questId);
    out.Append<uint32_t>(static_cast<uint32_t>(quest.questIcon));
    out.Append<int32_t>(quest.questLevel);
    out.Append<uint32_t>(quest.questFlags);
    out.Append<uint8_t>(quest.blueQuestionMark ? 1u : 0u);
    out.WriteString(quest.questTitle);
  }
  return out;
}

/// Trinity `QuestFailedReason` values for `SMSG_QUESTGIVER_INVALID_QUEST`.
inline constexpr uint32_t kInvalidReasonQuestAlreadyOn = 13u;
inline constexpr uint32_t kInvalidReasonQuestAlreadyDone = 7u;
inline constexpr uint32_t kInvalidReasonQuestWrongRace = 6u;

/// `SMSG_QUESTGIVER_INVALID_QUEST` — uint32 reason (`QuestFailedReason` in ref).
inline WorldPacket BuildQuestGiverInvalidQuest(uint32_t reason = 0u) {
  WorldPacket out(SMSG_QUESTGIVER_INVALID_QUEST, 8);
  out.Append<uint32_t>(reason);
  return out;
}

/// `SMSG_QUEST_LOG_FULL` — no payload on 4.3.4 (ref `QuestLogFull`).
inline WorldPacket BuildQuestLogFull() {
  WorldPacket out(SMSG_QUEST_LOG_FULL, 0);
  return out;
}

/// `SMSG_QUEST_QUERY_RESPONSE` — ref `QueryQuestInfoResponse::Write` (4.3.4).
inline WorldPacket BuildQuestQueryResponse(QuestGossipSummary const &summary) {
  WorldPacket out(SMSG_QUEST_QUERY_RESPONSE, 2000);

  out.Append<uint32_t>(summary.questId);
  out.Append<int32_t>(QuestQueryWireType(summary.flags));
  out.Append<int32_t>(summary.questLevel);
  out.Append<int32_t>(0); // QuestMinLevel
  out.Append<int32_t>(summary.questSortId);
  out.Append<int32_t>(0); // QuestInfoID
  out.Append<uint32_t>(0u); // SuggestedGroupNum

  for (uint32_t i = 0; i < kBgTeamsCount; ++i) {
    out.Append<uint32_t>(0u);
    out.Append<int32_t>(0);
  }

  out.Append<int32_t>(0); // RewardNextQuest
  out.Append<uint32_t>(0u); // RewardXPDifficulty

  if ((summary.flags & kQuestFlagsHiddenRewards) != 0)
    out.Append<int32_t>(0);
  else
    out.Append<int32_t>(0); // RewardMoney

  out.Append<uint32_t>(0u); // RewardBonusMoney
  out.Append<int32_t>(0);   // RewardDisplaySpell
  out.Append<int32_t>(0);   // RewardSpell
  out.Append<int32_t>(0);   // RewardHonor
  out.Append<float>(0.f);  // RewardKillHonor
  out.Append<uint32_t>(0u); // StartItem
  out.Append<uint32_t>(summary.flags);
  out.Append<uint32_t>(0u); // MinimapTargetMark
  out.Append<uint32_t>(0u); // RewardTitle
  out.Append<uint32_t>(0u); // RequiredPlayerKills
  out.Append<uint32_t>(0u); // RewardTalents
  out.Append<uint32_t>(0u); // RewardArenaPoints
  out.Append<uint32_t>(0u); // RewardSkillLineID
  out.Append<uint32_t>(0u); // RewardNumSkillUps
  out.Append<uint32_t>(0u); // RewardReputationMask
  out.Append<uint32_t>(0u); // PortraitGiver
  out.Append<uint32_t>(0u); // PortraitTurnIn

  for (uint32_t i = 0; i < kQuestRewardsCount; ++i) {
    out.Append<int32_t>(0);
    out.Append<uint32_t>(0u);
  }
  for (uint32_t i = 0; i < kQuestRewardChoicesCount; ++i) {
    out.Append<int32_t>(0);
    out.Append<uint32_t>(0u);
  }

  for (uint32_t i = 0; i < kQuestReputationsCount; ++i)
    out.Append<uint32_t>(0u);
  for (uint32_t i = 0; i < kQuestReputationsCount; ++i)
    out.Append<int32_t>(0);
  for (uint32_t i = 0; i < kQuestReputationsCount; ++i)
    out.Append<uint32_t>(0u);

  out.Append<uint32_t>(0u); // POIContinent
  out.Append<float>(0.f);
  out.Append<float>(0.f);
  out.Append<uint32_t>(0u); // POIPriority

  out.WriteString(summary.title);
  out.WriteString(summary.logDescription);
  out.WriteString(summary.questDescription);
  out.WriteString("");
  out.WriteString("");

  for (uint32_t i = 0; i < kQuestObjectivesCount; ++i) {
    out.Append<uint32_t>(0u);
    out.Append<uint32_t>(0u);
    out.Append<int32_t>(0);
    out.Append<uint32_t>(0u);
  }

  for (uint32_t i = 0; i < kQuestItemObjectivesCount; ++i) {
    out.Append<int32_t>(0);
    out.Append<uint32_t>(0u);
  }

  out.Append<uint32_t>(0u); // RequiredSpell

  for (uint32_t i = 0; i < kQuestObjectivesCount; ++i)
    out.WriteString("");

  for (uint32_t i = 0; i < kQuestRewardCurrencyCount; ++i) {
    out.Append<uint32_t>(0u);
    out.Append<uint32_t>(0u);
  }
  for (uint32_t i = 0; i < kQuestRequiredCurrencyCount; ++i) {
    out.Append<uint32_t>(0u);
    out.Append<uint32_t>(0u);
  }

  out.WriteString("");
  out.WriteString("");
  out.WriteString("");
  out.WriteString("");

  out.Append<uint32_t>(summary.soundAccept);
  out.Append<uint32_t>(summary.soundTurnIn);

  return out;
}

/// `SMSG_QUEST_POI_QUERY_RESPONSE` — empty POI list per quest (MVP until `quest_poi` import).
inline WorldPacket
BuildQuestPoiQueryResponse(std::vector<uint32_t> const &questIds) {
  WorldPacket out(SMSG_QUEST_POI_QUERY_RESPONSE, 4 + 8 * questIds.size());
  out.Append<uint32_t>(static_cast<uint32_t>(questIds.size()));
  for (uint32_t const questId : questIds)
    out.Append<uint32_t>(questId);
  for (uint32_t const questId : questIds) {
    (void)questId;
    out.Append<uint32_t>(0u); // POI count
  }
  return out;
}

/// `SMSG_QUEST_NPC_QUERY_RESPONSE` — ref `HandleQuestNPCQuery` (bit-packed header).
inline WorldPacket BuildQuestNpcQueryResponse(
    std::vector<std::pair<uint32_t, std::vector<uint32_t>>> const &quests) {
  WorldPacket out(SMSG_QUEST_NPC_QUERY_RESPONSE, 16 + quests.size() * 16);
  BitWriter bw(out);
  bw.WriteBits(static_cast<uint32_t>(quests.size()), 23);
  for (auto const &[questId, entries] : quests) {
    (void)questId;
    bw.WriteBits(static_cast<uint32_t>(entries.size()), 24);
  }
  bw.Flush();
  for (auto const &[questId, entries] : quests) {
    out.Append<uint32_t>(questId);
    for (uint32_t const entry : entries)
      out.Append<uint32_t>(entry);
  }
  return out;
}

/// `SMSG_PLAY_SOUND` — ref `WorldPackets::Misc::PlaySound` (4.3.4).
inline WorldPacket BuildPlaySound(uint64_t sourceObjectGuid, uint32_t soundKitId) {
  WorldPacket out(SMSG_PLAY_SOUND, 12);
  out.Append<uint32_t>(soundKitId);
  out.Append<uint64_t>(sourceObjectGuid);
  return out;
}

} // namespace Firelands::quest
