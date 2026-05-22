#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <infrastructure/dbc/SpellCastTablesDbc.h>
#include <vector>

using namespace Firelands;

namespace {

void AppendLeU32(std::vector<uint8_t> &b, uint32_t v) {
  b.push_back(static_cast<uint8_t>(v & 0xFFu));
  b.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
  b.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
  b.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

void AppendLeF32(std::vector<uint8_t> &b, float f) {
  static_assert(sizeof(float) == 4, "float size");
  uint32_t u = 0;
  std::memcpy(&u, &f, sizeof(u));
  AppendLeU32(b, u);
}

} // namespace

TEST(SpellCastTablesDbcSpellPowerTests, GetSpellPowerManaCostFromMinimalDbc) {
  std::filesystem::path const tmp =
      std::filesystem::temp_directory_path() / "firelands_ut_spellpower.dbc";

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), {'W', 'D', 'B', 'C'});
  AppendLeU32(raw, 1u);   // recordCount
  AppendLeU32(raw, 8u);   // fieldCount (fmt "diiiixxf")
  AppendLeU32(raw, 32u);  // recordSize
  AppendLeU32(raw, 0u);   // stringBlockSize
  // One row: id=9001, ManaCost=4242, three int32 zeros, two x padding, float 0
  AppendLeU32(raw, 9001u);
  AppendLeU32(raw, 4242u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u); // x
  AppendLeU32(raw, 0u); // x
  AppendLeF32(raw, 0.0f);

  ASSERT_EQ(raw.size(), 20u + 32u);

  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out);
    out.write(reinterpret_cast<char const *>(raw.data()),
              static_cast<std::streamsize>(raw.size()));
  }

  SpellCastTablesDbc tables;
  ASSERT_TRUE(tables.Load("", "", "", tmp.string(), ""));

  EXPECT_EQ(tables.GetSpellPowerManaCost(0u), 0u);
  EXPECT_EQ(tables.GetSpellPowerManaCost(9001u), 4242u);
  EXPECT_EQ(tables.GetSpellPowerManaCost(99999u), 0u);

  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}

TEST(SpellCastTablesDbcSpellPowerTests, GetSpellPowerManaCostUsesFloatWhenIntegerCostZero) {
  std::filesystem::path const tmp =
      std::filesystem::temp_directory_path() / "firelands_ut_spellpower_float.dbc";

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), {'W', 'D', 'B', 'C'});
  AppendLeU32(raw, 1u);
  AppendLeU32(raw, 8u);
  AppendLeU32(raw, 32u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 33u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u);
  AppendLeU32(raw, 0u);
  AppendLeF32(raw, 9.0f);

  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out);
    out.write(reinterpret_cast<char const *>(raw.data()),
              static_cast<std::streamsize>(raw.size()));
  }

  SpellCastTablesDbc tables;
  ASSERT_TRUE(tables.Load("", "", "", tmp.string(), ""));
  EXPECT_EQ(tables.GetSpellPowerManaCost(33u), 9u);

  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}
