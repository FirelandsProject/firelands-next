#include <shared/network/packets/server/GossipPackets.h>
#include <shared/network/packets/server/NpcTextPackets.h>
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
  EXPECT_EQ(copy.Read<int32_t>(), 2782);
  EXPECT_EQ(copy.Read<int32_t>(), 3466);
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
  copy.Read<int32_t>();
  copy.Read<int32_t>();
  EXPECT_EQ(copy.Read<uint32_t>(), 0u);
  EXPECT_EQ(copy.Read<uint32_t>(), 1u);
  EXPECT_EQ(copy.Read<int32_t>(), static_cast<int32_t>(quest.questId));
  EXPECT_EQ(copy.Read<int32_t>(), static_cast<int32_t>(quest.questIcon));
  EXPECT_EQ(copy.Read<int32_t>(), quest.questLevel);
  EXPECT_EQ(copy.Read<int32_t>(), static_cast<int32_t>(quest.questFlags));
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
}

} // namespace Firelands
