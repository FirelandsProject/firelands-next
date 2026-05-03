// Phase B parity: (1) deterministic .map bytes from MapFileWriter; (2) optional
// byte-for-byte check vs a reference golden when WoW Data + fixture are present.

#include "MapExtractorTask.h"
#include "MapFileWriter.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
namespace FVE = Firelands::VMap::MapExtractor;

namespace {

// Flat height 100 everywhere, single area id 0, no liquid/holes — 68-byte .map
// (matches MapFileWriter layout for kMapHeightNoHeight + kMapAreaNoArea).
static constexpr uint8_t kFlatHeight100Map15595[68] = {
    0x4d, 0x41, 0x50, 0x53, 0x0a, 0x00, 0x00, 0x00, 0xeb, 0x3c, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x52, 0x45, 0x41,
    0x01, 0x00, 0x00, 0x00, 0x48, 0x4d, 0x47, 0x54, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x42,
    0x00, 0x00, 0xc8, 0x42};

static bool ReadFileBytes(fs::path const& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) {
        return false;
    }
    auto const sz = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    out.resize(sz);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()),
                                    static_cast<std::streamsize>(sz)));
}

} // namespace

TEST(MapExtractorParity, WriterFlatHeight100MatchesGoldenBytes) {
    FVE::AdtGridData grid{};
    for (int y = 0; y <= FVE::kAdtGridSize; ++y) {
        for (int x = 0; x <= FVE::kAdtGridSize; ++x) {
            grid.V9[y][x] = 100.f;
        }
    }
    for (int y = 0; y < FVE::kAdtGridSize; ++y) {
        for (int x = 0; x < FVE::kAdtGridSize; ++x) {
            grid.V8[y][x] = 100.f;
        }
    }

    fs::path const path = fs::temp_directory_path() / "firelands_map_parity_flat.map";
    ASSERT_TRUE(FVE::MapFileWriter::Write(grid, path, 15595));

    std::vector<uint8_t> got;
    ASSERT_TRUE(ReadFileBytes(path, got));
    std::error_code ec;
    fs::remove(path, ec);

    ASSERT_EQ(got.size(), sizeof(kFlatHeight100Map15595));
    EXPECT_EQ(std::memcmp(got.data(), kFlatHeight100Map15595, got.size()), 0);
}

TEST(MapExtractorParity, OptionalGoldenTileMatchesReferenceExtractor) {
    char const* wowData = std::getenv("FIRELANDS_VMAP_PARITY_WOW_DATA");
    fs::path const golden =
        fs::path(FIRELANDS_TEST_DATA_DIR) / "tests/fixtures/vmap/0003232.golden.map";
    if (!wowData || !*wowData || !fs::is_directory(wowData) || !fs::exists(golden)) {
        GTEST_SKIP() << "Set FIRELANDS_VMAP_PARITY_WOW_DATA to WoW Data/ and place "
                        "tests/fixtures/vmap/0003232.golden.map from the reference "
                        "map_extractor (Azeroth tile 32,32).";
    }

    std::vector<uint8_t> expected;
    ASSERT_TRUE(ReadFileBytes(golden, expected));

    fs::path const outMap = fs::temp_directory_path() / "firelands_map_parity_live.map";
    FVE::MapExtractorOptions opts;
    opts.dataDir   = wowData;
    opts.outputDir = fs::temp_directory_path() / "firelands_map_parity_unused";
    opts.verbose   = false;

    int const rc = FVE::ExtractSingleServerMapTile(opts, 0, 32, 32, outMap);
    ASSERT_EQ(rc, 1) << "Could not extract Azeroth tile 32,32 (WDT/MPQ issue?)";

    std::vector<uint8_t> actual;
    ASSERT_TRUE(ReadFileBytes(outMap, actual));
    fs::remove(outMap);

    ASSERT_EQ(actual.size(), expected.size())
        << "Golden vs Firelands .map size mismatch — see docs/EN/VMAP_MAP_TILE_PARITY.md";
    if (actual != expected) {
        std::size_t firstDiff = 0;
        while (firstDiff < actual.size() && actual[firstDiff] == expected[firstDiff]) {
            ++firstDiff;
        }
        FAIL() << "Byte mismatch at offset " << firstDiff
               << " (document acceptable deltas in VMAP_MAP_TILE_PARITY.md if intentional).";
    }
}
