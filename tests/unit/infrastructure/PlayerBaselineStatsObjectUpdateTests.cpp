#include <domain/models/Character.h>
#include <gtest/gtest.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/network/UpdateFields.h>

#include <cstring>

using namespace Firelands;

namespace ws_obj = WorldSessionObjectUpdate;

namespace {

float ReadPackedFloat(uint32 bits) {
  float value = 0.f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

Character MakeMageWithStats() {
  Character ch(1u, 1u, "Mage", 1, 8, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
               true);
  std::array<uint32_t, 5> const prim{17u, 22u, 22u, 23u, 23u};
  ch.ApplyCombatStateFromTemplate(prim, 120u, 120u, 100u, 100u, 0);
  return ch;
}

} // namespace

TEST(PlayerBaselineStatsObjectUpdateTests, DamageAndHasteMultipliersDefaultToOne) {
  Character const ch = MakeMageWithStats();
  auto const fields = ws_obj::BuildPlayerUpdateFields(1u, ch);

  auto const pctIt =
      fields.find(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + 2));
  ASSERT_NE(pctIt, fields.end());
  EXPECT_FLOAT_EQ(ReadPackedFloat(pctIt->second), 1.0f);

  auto const hasteIt = fields.find(static_cast<uint16>(PLAYER_FIELD_MOD_HASTE));
  ASSERT_NE(hasteIt, fields.end());
  EXPECT_FLOAT_EQ(ReadPackedFloat(hasteIt->second), 1.0f);

  auto const castHasteIt = fields.find(static_cast<uint16>(UNIT_MOD_CAST_HASTE));
  ASSERT_NE(castHasteIt, fields.end());
  EXPECT_FLOAT_EQ(ReadPackedFloat(castHasteIt->second), 1.0f);
}

TEST(PlayerBaselineStatsObjectUpdateTests, PlayerCreateSetsRegeneratePowerFlag) {
  Character const ch = MakeMageWithStats();
  auto const fields = ws_obj::BuildPlayerUpdateFields(1u, ch);

  auto const flags2It = fields.find(static_cast<uint16>(UNIT_FIELD_FLAGS_2));
  ASSERT_NE(flags2It, fields.end());
  EXPECT_NE(flags2It->second & UNIT_FLAG2_REGENERATE_POWER, 0u);
}

TEST(PlayerBaselineStatsObjectUpdateTests, MageSpellPowerUsesIntellectPerSchool) {
  Character const ch = MakeMageWithStats();
  auto const fields = ws_obj::BuildPlayerUpdateFields(1u, ch);

  auto const fireIt =
      fields.find(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + 2));
  ASSERT_NE(fireIt, fields.end());
  EXPECT_EQ(fireIt->second, 23u);
}
