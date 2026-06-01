#include <shared/network/packets/server/GossipPackets.h>
#include <shared/network/packets/server/NpcTextPackets.h>
#include <shared/network/packets/server/QuestPackets.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <domain/models/QuestGossip.h>
#include <shared/game/WowGuid.h>
#include <shared/network/WorldOpcodes.h>
#include <gtest/gtest.h>

namespace Firelands {

namespace ws_obj = WorldSessionObjectUpdate;

TEST(GossipPacketTests, BuildGossipMessage_UsesFullGuidAndNullTerminatedStrings) {
  uint64_t const npcGuid = MakeCreatureObjectGuid(1383, 0x70000001u);
  GossipMenuItem item;
  item.optionIndex = 0;
  item.icon = GossipOptionIcon::Chat;
  item.optionText = "Test option";
  item.boxMessage = "Confirm";

  WorldPacket pkt =
      gossip::BuildGossipMessage(npcGuid, 2782, 3466, std::vector<GossipMenuItem>{item});

  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_GOSSIP_MESSAGE));

  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  EXPECT_EQ(copy.Read<uint64_t>(), npcGuid);
  EXPECT_EQ(copy.Read<uint32_t>(), 2782u);
  EXPECT_EQ(copy.Read<uint32_t>(), 3466u);
  EXPECT_EQ(copy.Read<uint32_t>(), 1u);
  EXPECT_EQ(copy.Read<int32_t>(), 0);
  EXPECT_EQ(copy.Read<uint8_t>(), static_cast<uint8_t>(GossipOptionIcon::Chat));
  EXPECT_EQ(copy.Read<int8_t>(), 0);
  EXPECT_EQ(copy.Read<int32_t>(), 0);
  std::string text;
  char c = 0;
  while (copy.GetReadPos() < copy.Size() &&
         (c = static_cast<char>(copy.Read<uint8_t>())) != 0)
    text += c;
  EXPECT_EQ(text, "Test option");
  EXPECT_EQ(copy.Read<uint32_t>(), 0u);
}

TEST(GossipPacketTests, BuildGossipMessage_WritesQuestLinesAfterOptions) {
  uint64_t const npcGuid = MakeCreatureObjectGuid(1383, 0x70000001u);
  GossipQuestItem quest;
  quest.questId = 999001;
  quest.questIcon = static_cast<uint8_t>(QuestGossipIcon::Available);
  quest.questLevel = 5;
  quest.questFlags = 0;
  quest.blueQuestionMark = false;
  quest.questTitle = "A Gossip Test Quest";

  WorldPacket pkt = gossip::BuildGossipMessage(npcGuid, 0, 1, {}, {quest});
  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  copy.Read<uint64_t>();
  copy.Read<uint32_t>();
  copy.Read<uint32_t>();
  EXPECT_EQ(copy.Read<uint32_t>(), 0u);
  EXPECT_EQ(copy.Read<uint32_t>(), 1u);
  EXPECT_EQ(copy.Read<uint32_t>(), quest.questId);
  EXPECT_EQ(copy.Read<uint32_t>(), static_cast<uint32_t>(quest.questIcon));
  EXPECT_EQ(copy.Read<int32_t>(), quest.questLevel);
  EXPECT_EQ(copy.Read<uint32_t>(), quest.questFlags);
  EXPECT_EQ(copy.Read<uint8_t>(), 0u);
  std::string title;
  char c = 0;
  while (copy.GetReadPos() < copy.Size() &&
         (c = static_cast<char>(copy.Read<uint8_t>())) != 0)
    title += c;
  EXPECT_EQ(title, quest.questTitle);
}

TEST(GossipPacketTests, ReadClientTargetGuid_ReadsFullUint64ForGossipHello) {
  uint64_t const guid = MakeCreatureObjectGuid(1383, 42u);
  WorldPacket pkt;
  pkt.Append<uint64_t>(guid);

  EXPECT_EQ(ws_obj::ReadClientTargetGuid(pkt), guid);
  EXPECT_EQ(pkt.GetReadPos(), pkt.Size());
}

TEST(GossipPacketTests, ReadClientTargetGuid_ReadsPackedGuidWhenShorter) {
  uint64_t const guid = MakeCreatureObjectGuid(68, 1u);
  WorldPacket pkt;
  pkt.WritePackedGuid(guid);

  EXPECT_EQ(ws_obj::ReadClientTargetGuid(pkt), guid);
  EXPECT_EQ(pkt.GetReadPos(), pkt.Size());
}

TEST(GossipPacketTests, BuildNpcTextUpdate_Fallback_IncludesTextIdAndGreeting) {
  WorldPacket pkt = gossip::BuildNpcTextUpdate(3466);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_NPC_TEXT_UPDATE));

  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  EXPECT_EQ(copy.Read<uint32_t>(), 3466u);
  EXPECT_FLOAT_EQ(copy.Read<float>(), 1.0f);
  std::string line;
  char c = 0;
  while (copy.GetReadPos() < copy.Size() &&
         (c = static_cast<char>(copy.Read<uint8_t>())) != 0)
    line += c;
  EXPECT_EQ(line, "Greetings $N");
}

TEST(GossipPacketTests, BuildQuestGiverOfferReward_WritesGuidTitleAndRewards) {
  QuestGossipSummary summary;
  summary.title = "The Rise of the Darkspear";
  summary.flags = 0x00080000u;
  summary.questDescription = "Well done, $n.";

  WorldPacket pkt =
      quest::BuildQuestGiverOfferReward(0xABCD, 24764, summary, "Well done, $n.");
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_QUESTGIVER_OFFER_REWARD));

  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  EXPECT_EQ(copy.Read<uint64_t>(), 0xABCDu);
  EXPECT_EQ(copy.Read<uint32_t>(), 24764u);
  auto readCString = [&copy]() {
    std::string s;
    char c = 0;
    while (copy.GetReadPos() < copy.Size() &&
           (c = static_cast<char>(copy.Read<uint8_t>())) != 0)
      s += c;
    return s;
  };
  EXPECT_EQ(readCString(), "The Rise of the Darkspear");
  EXPECT_EQ(readCString(), "Well done, $n.");
}

TEST(GossipPacketTests, BuildQuestGiverQuestDetails_WritesBodyAndObjectives) {
  QuestGossipSummary summary;
  summary.title = "Quest Title";
  summary.questDescription = "Story body";
  summary.logDescription = "Kill 10 wolves";
  summary.flags = kQuestFlagAutoAccept;

  WorldPacket pkt = quest::BuildQuestGiverQuestDetails(0x1234, 42, summary);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_QUESTGIVER_QUEST_DETAILS));

  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  copy.Read<uint64_t>();
  copy.Read<uint64_t>();
  EXPECT_EQ(copy.Read<uint32_t>(), 42u);
  auto readCString = [&copy]() {
    std::string s;
    char c = 0;
    while (copy.GetReadPos() < copy.Size() &&
           (c = static_cast<char>(copy.Read<uint8_t>())) != 0)
      s += c;
    return s;
  };
  EXPECT_EQ(readCString(), "Quest Title");
  EXPECT_EQ(readCString(), "Story body");
  EXPECT_EQ(readCString(), "Kill 10 wolves");

  copy.SetReadPos(0);
  copy.Read<uint64_t>();
  copy.Read<uint64_t>();
  copy.Read<uint32_t>();
  for (int i = 0; i < 3; ++i) {
    while (copy.GetReadPos() < copy.Size() &&
           static_cast<char>(copy.Read<uint8_t>()) != 0) {
    }
  }
  for (int i = 0; i < 4; ++i) {
    while (copy.GetReadPos() < copy.Size() &&
           static_cast<char>(copy.Read<uint8_t>()) != 0) {
    }
  }
  copy.Read<uint32_t>();
  copy.Read<uint32_t>();
  copy.Read<uint8_t>();
  EXPECT_EQ(copy.Read<uint32_t>(), 0u);
}

TEST(GossipPacketTests, BuildNpcTextUpdate_FromDomainRow_WritesCustomGreeting) {
  NpcText text;
  text.id = 99;
  text.options[0].probability = 1.f;
  text.options[0].text0 = "Custom line";
  text.options[0].text1 = "";

  WorldPacket pkt = gossip::BuildNpcTextUpdate(text);
  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  EXPECT_EQ(copy.Read<uint32_t>(), 99u);
  EXPECT_FLOAT_EQ(copy.Read<float>(), 1.0f);
  std::string line;
  char c = 0;
  while (copy.GetReadPos() < copy.Size() &&
         (c = static_cast<char>(copy.Read<uint8_t>())) != 0)
    line += c;
  EXPECT_EQ(line, "Custom line");
  while (copy.GetReadPos() < copy.Size() &&
         (c = static_cast<char>(copy.Read<uint8_t>())) != 0) {
  }
  EXPECT_EQ(copy.Read<uint32_t>(), 0u);
}

TEST(GossipPacketTests, BuildNpcTextUpdate_LanguageIsUint32PerSlot) {
  NpcText text;
  text.id = 1;
  text.options[0].probability = 1.f;
  text.options[0].text0 = "Hi";
  text.options[0].text1 = "Hi";
  text.options[0].language = 7;

  WorldPacket pkt = gossip::BuildNpcTextUpdate(text);
  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  copy.Read<uint32_t>();
  copy.Read<float>();
  while (copy.GetReadPos() < copy.Size() &&
         static_cast<char>(copy.Read<uint8_t>()) != 0) {
  }
  while (copy.GetReadPos() < copy.Size() &&
         static_cast<char>(copy.Read<uint8_t>()) != 0) {
  }
  EXPECT_EQ(copy.Read<uint32_t>(), 7u);
}

TEST(GossipPacketTests, BuildQuestQueryResponse_EncodesQuestHeader) {
  QuestGossipSummary summary;
  summary.questId = 24607u;
  summary.title = "The Rise of the Darkspear";
  summary.questLevel = 1;
  summary.questSortId = 368;
  summary.flags = 0x00080000u;

  WorldPacket pkt = quest::BuildQuestQueryResponse(summary);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_QUEST_QUERY_RESPONSE));

  WorldPacket copy = pkt;
  copy.SetReadPos(0);
  EXPECT_EQ(copy.Read<uint32_t>(), 24607u);
  EXPECT_EQ(copy.Read<int32_t>(), 0); // autocomplete wire type
  EXPECT_EQ(copy.Read<int32_t>(), 1); // level
  EXPECT_EQ(copy.Read<int32_t>(), 0); // QuestMinLevel
  EXPECT_EQ(copy.Read<int32_t>(), 368); // QuestSortID (Echo Isles)
}

TEST(GossipPacketTests, BuildQuestPoiQueryResponse_EmptyPoiPerQuest) {
  WorldPacket pkt = quest::BuildQuestPoiQueryResponse({24764u});
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_QUEST_POI_QUERY_RESPONSE));
  pkt.SetReadPos(0);
  EXPECT_EQ(pkt.Read<uint32_t>(), 1u);
  EXPECT_EQ(pkt.Read<uint32_t>(), 24764u);
  EXPECT_EQ(pkt.Read<uint32_t>(), 0u);
}

TEST(GossipPacketTests, BuildQuestNpcQueryResponse_EncodesCreatureList) {
  WorldPacket pkt =
      quest::BuildQuestNpcQueryResponse({{24764u, {37951u, 38243u}}});
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_QUEST_NPC_QUERY_RESPONSE));
  EXPECT_GT(pkt.Size(), 8u);
}

TEST(GossipPacketTests, BuildPlaySound_EncodesOpcodeAndKit) {
  WorldPacket pkt = quest::BuildPlaySound(0x42, 890u);
  EXPECT_EQ(pkt.GetOpcode(), static_cast<uint32>(SMSG_PLAY_SOUND));
  pkt.SetReadPos(0);
  EXPECT_EQ(pkt.Read<uint32_t>(), 890u);
  EXPECT_EQ(pkt.Read<uint64_t>(), 0x42u);
}

} // namespace Firelands
