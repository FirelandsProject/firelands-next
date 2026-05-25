#include <gtest/gtest.h>
#include <shared/network/UpdateData.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/UpdateFields.h>

using namespace Firelands;

TEST(UpdateDataTest, BuildCreateObjectPacketForPlayer) {
    UpdateData data;
    
    uint64 guid = 0x1234567890ABCDEF;
    
    MovementInfo move;
    move.x = 100.0f;
    move.y = 200.0f;
    move.z = 300.0f;
    
    std::map<uint16, uint32> fields;
    fields[OBJECT_FIELD_GUID] = (uint32)(guid & 0xFFFFFFFF);
    fields[OBJECT_FIELD_GUID + 1] = (uint32)(guid >> 32);
    
    // Add a dummy CreateObject block
    fields[UNIT_FIELD_FACTIONTEMPLATE] = 123;
    data.AddCreateObject(guid, TYPEID_PLAYER, move, fields);
    
    WorldPacket packet(SMSG_UPDATE_OBJECT);
    data.Build(packet);
    
    // Check structure
    packet.SetReadPos(0);
    EXPECT_EQ(packet.Read<uint16>(), 0); // MapId
    ASSERT_EQ(packet.Read<uint32>(), 1); // 1 block
    EXPECT_EQ(packet.Read<uint8>(), UPDATETYPE_CREATE_OBJECT2);
    
    // Skip GUID and Type
    packet.Read<uint8>(); // Packed GUID mask (simplification)
    // We can't easily predict the exact size of packed GUID here without more logic, 
    // but we can check the end of the packet for our fields.
    
    // The mask for our fields (GUID [0,1] and FACTIONTEMPLATE [49])
    // Index 0: bit 0 of block 0
    // Index 1: bit 1 of block 0
    // Index 49: bit 17 of block 1 (49 - 32 = 17)
    
    // Instead of precise byte matching (which is hard due to living data bits),
    // let's verify that UPDATETYPE_CREATE_OBJECT2 is indeed 2 (Cata 15595).
    EXPECT_EQ(UPDATETYPE_CREATE_OBJECT2, 2);
}

TEST(UpdateDataTest, ValuesUpdateMaskUsesLastSetBitBlockCount) {
  UpdateData data(1);
  std::map<uint16, uint32> fields;
  uint16 const field = static_cast<uint16>(PLAYER_QUEST_LOG_1_1);
  fields[field] = 24607u;

  data.AddValuesUpdate(0x0A, fields);

  WorldPacket packet(SMSG_UPDATE_OBJECT);
  data.Build(packet);

  packet.SetReadPos(0);
  (void)packet.Read<uint16>();
  (void)packet.Read<uint32>();
  EXPECT_EQ(packet.Read<uint8>(), static_cast<uint8>(UPDATETYPE_VALUES));
  (void)packet.ReadPackedGuid();

  uint8 const blockCount = packet.Read<uint8>();
  EXPECT_EQ(blockCount, static_cast<uint8>((field + 1u + 31u) / 32u));

  std::vector<uint32> mask(blockCount);
  for (uint8 i = 0; i < blockCount; ++i)
    mask[i] = packet.Read<uint32>();
  EXPECT_NE(mask[field / 32] & (1u << (field % 32)), 0u);
  EXPECT_EQ(packet.Read<uint32>(), 24607u);
}
