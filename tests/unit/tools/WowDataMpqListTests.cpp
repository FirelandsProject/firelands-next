#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "WowDataMpqList.h"

namespace fs = std::filesystem;

static fs::path UniqueTempDir() {
  const auto tick =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return fs::temp_directory_path() /
         fs::path("firelands-wow-mpq-test-" + std::to_string(tick));
}

static void Touch(const fs::path &p) {
  std::ofstream f(p);
  f << "x";
}

TEST(WowDataMpqList, orders_common_before_patches_and_numeric_patch) {
  const fs::path dir = UniqueTempDir();
  ASSERT_TRUE(fs::create_directories(dir));
  Touch(dir / "patch-10.MPQ");
  Touch(dir / "patch-2.MPQ");
  Touch(dir / "common.MPQ");
  Touch(dir / "expansion.MPQ");

  const auto list = firelands::extract::BuildCataclysmMpqOpenOrder(dir);
  ASSERT_EQ(list.size(), 4u);
  EXPECT_EQ(list[0].filename(), "common.MPQ");
  EXPECT_EQ(list[1].filename(), "expansion.MPQ");
  EXPECT_EQ(list[2].filename(), "patch-2.MPQ");
  EXPECT_EQ(list[3].filename(), "patch-10.MPQ");

  fs::remove_all(dir);
}

TEST(WowDataMpqList, ignores_non_mpq) {
  const fs::path dir = UniqueTempDir();
  ASSERT_TRUE(fs::create_directories(dir));
  Touch(dir / "readme.txt");
  Touch(dir / "common.MPQ");

  const auto list = firelands::extract::BuildCataclysmMpqOpenOrder(dir);
  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].filename(), "common.MPQ");

  fs::remove_all(dir);
}
