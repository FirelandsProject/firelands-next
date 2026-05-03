#include <gtest/gtest.h>
#include <shared/dbc/ItemDb2Wdb2.h>
#include <shared/Logger.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

void AppendU32(std::vector<uint8_t> &b, uint32_t v) {
  b.push_back(static_cast<uint8_t>(v & 0xFFu));
  b.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
  b.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
  b.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

} // namespace

TEST(ItemDb2Wdb2, LoadsSyntheticWdb2SingleRecord) {
  using namespace Firelands;
  if (!Logger::IsInitialized()) {
    Logger::Init(LoggerBuilder().WithConsole(false).Build());
  }
  std::vector<uint8_t> raw;
  AppendU32(raw, (uint32_t('W')) | (uint32_t('D') << 8) | (uint32_t('B') << 16) |
                      (uint32_t('2') << 24));
  AppendU32(raw, 1u);
  AppendU32(raw, 8u);
  AppendU32(raw, 32u);
  AppendU32(raw, 8u);
  AppendU32(raw, 0u);
  AppendU32(raw, 15595u);
  AppendU32(raw, 0u);
  AppendU32(raw, 0u);
  AppendU32(raw, 0u);
  AppendU32(raw, 0u);
  AppendU32(raw, 0u);
  AppendU32(raw, 99999u);
  AppendU32(raw, 4u);
  AppendU32(raw, 1u);
  AppendU32(raw, 0xFFFFFFFFu);
  AppendU32(raw, 7u);
  AppendU32(raw, 12345u);
  AppendU32(raw, 20u);
  AppendU32(raw, 0u);
  for (int i = 0; i < 8; ++i)
    raw.push_back(0);

  std::filesystem::path const p =
      std::filesystem::temp_directory_path() / "firelands_item_db2_unit.bin";
  {
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<char const *>(raw.data()),
              static_cast<std::streamsize>(raw.size()));
  }

  ItemDb2Wdb2 db;
  ASSERT_TRUE(db.Load(p.string()));
  auto const row = db.Lookup(99999u);
  ASSERT_TRUE(row);
  EXPECT_EQ(row->displayId, 12345u);
  EXPECT_EQ(row->inventoryType, 20u);
  std::error_code ec;
  std::filesystem::remove(p, ec);
}
