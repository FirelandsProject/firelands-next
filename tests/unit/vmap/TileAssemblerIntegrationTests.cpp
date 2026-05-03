// Integration tests for vmap4_assembler (Phase D, plan §9):
// synthetic Buildings/ → vmaps/ with valid VMAP_4.8 headers.

#include "TileAssembler.h"
#include "VMapMagic.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace {

using Firelands::VMap::kChunkNode;
using Firelands::VMap::kChunkWmod;
using Firelands::VMap::kChunkGrp;
using Firelands::VMap::kChunkIndx;
using Firelands::VMap::kChunkVert;
using Firelands::VMap::kModM2;
using Firelands::VMap::kRawVmapMagic;
using Firelands::VMap::kTileSize;
using Firelands::VMap::kVmapMagic;

// Minimal M2-style raw file matching tools/vmap/vmap4_extractor/model.cpp layout
// so WorldModel_Raw::Read + TileAssembler::convertRawFile accept it.
void WriteM2LikeRawVmapFile(fs::path const& path) {
    FILE* out = std::fopen(path.string().c_str(), "wb");
    ASSERT_NE(out, nullptr);

    ASSERT_EQ(std::fwrite(kRawVmapMagic, 1, 8, out), static_cast<size_t>(8));

    int32_t const nVertices = 3;
    ASSERT_EQ(std::fwrite(&nVertices, sizeof(nVertices), 1, out), static_cast<size_t>(1));

    uint32_t const nofGroups = 1;
    ASSERT_EQ(std::fwrite(&nofGroups, sizeof(nofGroups), 1, out), static_cast<size_t>(1));

    int32_t const zeros12[3] = {0, 0, 0};
    ASSERT_EQ(std::fwrite(zeros12, sizeof(zeros12), 1, out), static_cast<size_t>(1));

    // AABB (6 floats) — GroupModel_Raw overlays the tail of zeros + this box (see plan §4.3).
    float const bounds[6] = {0.f, 0.f, 0.f, 2.f, 2.f, 0.f};
    ASSERT_EQ(std::fwrite(bounds, sizeof(float), 6, out), static_cast<size_t>(6));

    int32_t const liquidFlags = 0;
    ASSERT_EQ(std::fwrite(&liquidFlags, sizeof(liquidFlags), 1, out), static_cast<size_t>(1));

    ASSERT_EQ(std::fwrite(kChunkGrp, 1, 4, out), static_cast<size_t>(4));
    uint32_t const branches = 1;
    int32_t const grpWsize  = static_cast<int32_t>(sizeof(branches) + sizeof(uint32_t) * branches);
    ASSERT_EQ(std::fwrite(&grpWsize, sizeof(grpWsize), 1, out), static_cast<size_t>(1));
    ASSERT_EQ(std::fwrite(&branches, sizeof(branches), 1, out), static_cast<size_t>(1));

    uint32_t const nIndexes = 3;
    ASSERT_EQ(std::fwrite(&nIndexes, sizeof(nIndexes), 1, out), static_cast<size_t>(1));

    ASSERT_EQ(std::fwrite(kChunkIndx, 1, 4, out), static_cast<size_t>(4));
    int32_t const indxWsize =
        static_cast<int32_t>(sizeof(uint32_t) + sizeof(uint16_t) * nIndexes);
    ASSERT_EQ(std::fwrite(&indxWsize, sizeof(indxWsize), 1, out), static_cast<size_t>(1));
    ASSERT_EQ(std::fwrite(&nIndexes, sizeof(nIndexes), 1, out), static_cast<size_t>(1));

    uint16_t const indices[3] = {0, 1, 2};
    ASSERT_EQ(std::fwrite(indices, sizeof(uint16_t), nIndexes, out),
              static_cast<size_t>(nIndexes));

    ASSERT_EQ(std::fwrite(kChunkVert, 1, 4, out), static_cast<size_t>(4));
    int32_t const vertWsize =
        static_cast<int32_t>(sizeof(int32_t) + sizeof(float) * 3u * static_cast<uint32_t>(nVertices));
    ASSERT_EQ(std::fwrite(&vertWsize, sizeof(vertWsize), 1, out), static_cast<size_t>(1));
    ASSERT_EQ(std::fwrite(&nVertices, sizeof(nVertices), 1, out), static_cast<size_t>(1));

    float const verts[9] = {0.f, 0.f, 0.f, 2.f, 0.f, 0.f, 0.f, 2.f, 0.f};
    ASSERT_EQ(std::fwrite(verts, sizeof(float), 9, out), static_cast<size_t>(9));

    std::fclose(out);
}

void WriteDirBinOneM2Spawn(fs::path const& dirBinPath, float posX, float posY, float posZ,
                           char const* modelName) {
    FILE* df = std::fopen(dirBinPath.string().c_str(), "wb");
    ASSERT_NE(df, nullptr);

    uint32_t const mapId = 0;
    ASSERT_EQ(std::fwrite(&mapId, sizeof(mapId), 1, df), static_cast<size_t>(1));

    uint8_t const flags = kModM2;
    uint8_t const adtId = 0;
    uint32_t const uniqueId = 1;
    float const    pos[3]   = {posX, posY, posZ};
    float const    rot[3]   = {0.f, 0.f, 0.f};
    float const    scale    = 1.f;

    ASSERT_EQ(std::fwrite(&flags, sizeof(flags), 1, df), static_cast<size_t>(1));
    ASSERT_EQ(std::fwrite(&adtId, sizeof(adtId), 1, df), static_cast<size_t>(1));
    ASSERT_EQ(std::fwrite(&uniqueId, sizeof(uniqueId), 1, df), static_cast<size_t>(1));
    ASSERT_EQ(std::fwrite(pos, sizeof(float), 3, df), static_cast<size_t>(3));
    ASSERT_EQ(std::fwrite(rot, sizeof(float), 3, df), static_cast<size_t>(3));
    ASSERT_EQ(std::fwrite(&scale, sizeof(scale), 1, df), static_cast<size_t>(1));

    uint32_t const nameLen = static_cast<uint32_t>(std::strlen(modelName));
    ASSERT_EQ(std::fwrite(&nameLen, sizeof(nameLen), 1, df), static_cast<size_t>(1));
    ASSERT_EQ(std::fwrite(modelName, 1, nameLen, df), static_cast<size_t>(nameLen));

    std::fclose(df);
}

} // namespace

TEST(TileAssemblerIntegration, ProducesVmtreeVmtileAndVmoWithVmap48Magic) {
    fs::path const root =
        fs::path(::testing::TempDir()) / "firelands_vmap4_assembler_it";
    fs::path const buildings = root / "Buildings";
    fs::path const vmaps     = root / "vmaps";
    std::error_code ec;
    fs::remove_all(root, ec);
    ASSERT_TRUE(fs::create_directories(buildings));

    char const kModel[] = "fixturem2";
    WriteM2LikeRawVmapFile(buildings / kModel);

    // Place instance inside ADT tile (32, 32) so 000_32_32.vmtile is created.
    float const px = 32.f * kTileSize + 50.f;
    float const py = 32.f * kTileSize + 50.f;
    float const pz = 0.f;
    WriteDirBinOneM2Spawn(buildings / "dir_bin", px, py, pz, kModel);

    Firelands::VMap::Assembler::TileAssembler assembler(buildings.string(), vmaps.string());
    ASSERT_TRUE(assembler.convertWorld2());

    fs::path const vmtree = vmaps / "000.vmtree";
    ASSERT_TRUE(fs::exists(vmtree));

    FILE* rf = std::fopen(vmtree.string().c_str(), "rb");
    ASSERT_NE(rf, nullptr);
    char magic[8]{};
    ASSERT_EQ(std::fread(magic, 1, 8, rf), static_cast<size_t>(8));
    EXPECT_EQ(std::memcmp(magic, kVmapMagic, 8), 0);
    char nodeTag[4]{};
    ASSERT_EQ(std::fread(nodeTag, 1, 4, rf), static_cast<size_t>(4));
    EXPECT_EQ(std::memcmp(nodeTag, kChunkNode, 4), 0);
    std::fclose(rf);

    fs::path const vmtile = vmaps / "000_32_32.vmtile";
    ASSERT_TRUE(fs::exists(vmtile));
    FILE* tf = std::fopen(vmtile.string().c_str(), "rb");
    ASSERT_NE(tf, nullptr);
    char tmagic[8]{};
    ASSERT_EQ(std::fread(tmagic, 1, 8, tf), static_cast<size_t>(8));
    EXPECT_EQ(std::memcmp(tmagic, kVmapMagic, 8), 0);
    uint32_t nSpawns = 0;
    ASSERT_EQ(std::fread(&nSpawns, sizeof(nSpawns), 1, tf), static_cast<size_t>(1));
    EXPECT_GT(nSpawns, 0u);
    std::fclose(tf);

    fs::path const vmo = vmaps / (std::string(kModel) + ".vmo");
    ASSERT_TRUE(fs::exists(vmo));
    FILE* vf = std::fopen(vmo.string().c_str(), "rb");
    ASSERT_NE(vf, nullptr);
    char vmagic[8]{};
    ASSERT_EQ(std::fread(vmagic, 1, 8, vf), static_cast<size_t>(8));
    EXPECT_EQ(std::memcmp(vmagic, kVmapMagic, 8), 0);
    char wmod[4]{};
    ASSERT_EQ(std::fread(wmod, 1, 4, vf), static_cast<size_t>(4));
    EXPECT_EQ(std::memcmp(wmod, kChunkWmod, 4), 0);
    std::fclose(vf);
}
