#include <gtest/gtest.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldOpcodes.h>

using namespace Firelands;

TEST(SpellCastWireTests, TryReadClientCancelCast) {
  WorldPacket p(CMSG_CANCEL_CAST, 8);
  p.Append<uint32>(774u);
  p.Append<uint8>(2u);
  uint32 spellId = 0;
  uint8 castId = 0;
  ASSERT_TRUE(SpellCastWire::TryReadClientCancelCast(p, spellId, castId));
  EXPECT_EQ(spellId, 774u);
  EXPECT_EQ(castId, 2u);
}

TEST(SpellCastWireTests, TryReadClientCastSpell_MinimalPayload) {
  WorldPacket p(CMSG_CAST_SPELL, 64);
  p.Append<uint8>(3);   // cast id
  p.Append<int32>(6673); // Battle Shout
  p.Append<int32>(0);  // misc
  p.Append<uint8>(0);  // send cast flags
  p.Append<uint32>(0); // target flags

  SpellCastWire::ClientCastSpellData c;
  ASSERT_TRUE(SpellCastWire::TryReadClientCastSpell(p, c));
  EXPECT_EQ(c.castId, 3);
  EXPECT_EQ(c.spellId, 6673);
  EXPECT_EQ(c.misc, 0);
  EXPECT_EQ(c.sendCastFlags, 0);
  EXPECT_EQ(c.targetFlags, 0u);
  EXPECT_EQ(c.unitTargetGuid, 0u);
}

TEST(SpellCastWireTests, ResolveSpellGoTimestampPrefersClientMovementTime) {
  EXPECT_EQ(SpellCastWire::ResolveSpellGoTimestampMs(42'000'000u), 42'000'000u);
  EXPECT_NE(SpellCastWire::ResolveSpellGoTimestampMs(0u), 0u);
}

TEST(SpellCastWireTests, TryReadClientCastSpell_WithTrajectory) {
  WorldPacket p(CMSG_CAST_SPELL, 64);
  p.Append<uint8>(1);
  p.Append<int32>(133);
  p.Append<int32>(0);
  p.Append<uint8>(SpellCastWire::CLIENT_CAST_FLAG_HAS_TRAJECTORY);
  p.Append<uint32>(SpellCastWire::TARGET_FLAG_UNIT);
  p.AppendPackGUID(0x50ULL);
  p.Append<float>(0.25f);
  p.Append<float>(24.f);
  p.Append<uint8>(0);

  SpellCastWire::ClientCastSpellData c;
  ASSERT_TRUE(SpellCastWire::TryReadClientCastSpell(p, c));
  EXPECT_TRUE(c.hasMissileTrajectory);
  EXPECT_FLOAT_EQ(c.missilePitch, 0.25f);
  EXPECT_FLOAT_EQ(c.missileSpeed, 24.f);
}

TEST(SpellCastWireTests, BuildSpellGo_AppendsMissileTrajectoryAfterTargets) {
  uint64 const guid = 0x0000000000ABCDEFULL;
  WorldPacket goNoMissile;
  std::vector<uint64> hits = {guid};
  SpellCastWire::BuildSpellGo(goNoMissile, guid, 1, 6673,
                              SpellCastWire::CAST_FLAG_UNKNOWN_9, 0, 99u, hits,
                              SpellCastWire::TARGET_FLAG_UNIT, guid);

  SpellCastWire::SpellMissileTrajectoryWire missile{};
  missile.pitch = 0.5f;
  missile.travelTimeMs = 400u;
  WorldPacket goMissile;
  uint32 const flags =
      SpellCastWire::BuildSpellGoCastFlags(true);
  SpellCastWire::BuildSpellGo(goMissile, guid, 1, 6673, flags, 0, 99u, hits,
                              SpellCastWire::TARGET_FLAG_UNIT, guid, &missile);
  EXPECT_EQ(goMissile.Size(), goNoMissile.Size() + sizeof(float) + sizeof(uint32));

  goMissile.SetReadPos(goNoMissile.Size());
  EXPECT_FLOAT_EQ(goMissile.Read<float>(), 0.5f);
  EXPECT_EQ(goMissile.Read<int32>(), 400);
}

static uint64 ReadFirstSpellGoHitGuid(WorldPacket &p) {
  p.SetReadPos(0);
  (void)p.ReadPackedGuid();
  (void)p.ReadPackedGuid();
  (void)p.Read<uint8>();
  (void)p.Read<uint32>();
  (void)p.Read<uint32>();
  (void)p.Read<uint32>();
  (void)p.Read<uint32>();
  (void)p.Read<uint32>();
  EXPECT_EQ(p.Read<uint8>(), 1u);
  return p.Read<uint64>();
}

TEST(SpellCastWireTests, BuildSpellGo_HitTargetsUseRawUint64Guid) {
  uint64 const guid = 0x0000000000ABCDEFULL;
  WorldPacket go;
  std::vector<uint64> hits = {guid};
  SpellCastWire::BuildSpellGo(go, guid, 1, 133, SpellCastWire::CAST_FLAG_UNKNOWN_9, 0,
                              99u, hits, SpellCastWire::TARGET_FLAG_UNIT, guid);
  EXPECT_EQ(ReadFirstSpellGoHitGuid(go), guid);
}

TEST(SpellCastWireTests, ResolveSpellGoMissile_UsesClientPitchAndSpeed) {
  SpellCastWire::ClientCastSpellData client{};
  client.hasMissileTrajectory = true;
  client.missilePitch = 0.3f;
  client.missileSpeed = 20.f;
  auto const resolved = SpellCastWire::ResolveSpellGoMissile(
      client, true, 0.f, 0.f, 0.f, true, 20.f, 0.f, 0.f, 0u);
  ASSERT_TRUE(resolved.sendOnWire);
  EXPECT_FLOAT_EQ(resolved.trajectory.pitch, 0.3f);
  EXPECT_EQ(resolved.trajectory.travelTimeMs, 1000u);

  client.missileSpeed = 0.f;
  EXPECT_FALSE(SpellCastWire::ResolveSpellGoMissile(client, true, 0.f, 0.f, 0.f, true,
                                                    20.f, 0.f, 0.f, 0u)
                   .sendOnWire);
}

TEST(SpellCastWireTests, BuildSpellStartAndGo_RoundtripSize) {
  uint64 const guid = 0x0000000000ABCDEFULL;
  WorldPacket start;
  SpellCastWire::BuildSpellStart(start, guid, 1, 6673,
                                 SpellCastWire::CAST_FLAG_HAS_TRAJECTORY, 0, 0,
                                 SpellCastWire::TARGET_FLAG_UNIT, guid);
  EXPECT_EQ(start.GetOpcode(), SMSG_SPELL_START);
  EXPECT_GT(start.Size(), 16u);

  WorldPacket go;
  std::vector<uint64> hits = {guid};
  auto const nowMs = uint32_t{12345678};
  SpellCastWire::BuildSpellGo(go, guid, 1, 6673,
                              SpellCastWire::CAST_FLAG_UNKNOWN_9, 0, nowMs,
                              hits, SpellCastWire::TARGET_FLAG_UNIT, guid);
  EXPECT_EQ(go.GetOpcode(), SMSG_SPELL_GO);
  EXPECT_GT(go.Size(), start.Size());
}
