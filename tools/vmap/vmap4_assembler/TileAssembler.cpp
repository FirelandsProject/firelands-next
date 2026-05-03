#include "TileAssembler.h"

#include "WorldModelWrite.h"

#include "../common/BoundingIntervalHierarchy.h"
#include "../common/Mat3.h"
#include "../common/VMapMagic.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <map>
#include <vector>

namespace fs = std::filesystem;

namespace Firelands::VMap::Assembler {

using Firelands::VMap::AaBox3;
using Firelands::VMap::BIH;
using Firelands::VMap::FPi;
using Firelands::VMap::kChunkNode;
using Firelands::VMap::kChunkSidx;
using Firelands::VMap::kGameObjectModels;
using Firelands::VMap::kModHasBound;
using Firelands::VMap::kModM2;
using Firelands::VMap::kModParentSpawn;
using Firelands::VMap::kRawVmapMagic;
using Firelands::VMap::kTileSize;
using Firelands::VMap::kVmapMagic;
using Firelands::VMap::Mat3;
using Firelands::VMap::ModelSpawn;
using Firelands::VMap::Vec3;

namespace {

Vec3 BoxCorner(AaBox3 const& b, int i) {
    return Vec3((i & 1) ? b.hi.x : b.lo.x, (i & 2) ? b.hi.y : b.lo.y,
                (i & 4) ? b.hi.z : b.lo.z);
}

uint32_t PackTileId(uint32_t tileX, uint32_t tileY) {
    return (tileX << 16) | (tileY & 0xFFFFu);
}

void UnpackTileId(uint32_t id, uint32_t& tileX, uint32_t& tileY) {
    tileX = id >> 16;
    tileY = id & 0xFFFFu;
}

struct ModelPosition {
    Mat3  iRotation{};
    Vec3  iPos{};
    Vec3  iDir{};
    float iScale{1.f};

    void init() {
        float const deg2rad = FPi() / 180.f;
        iRotation = Mat3::FromEulerAnglesZYX(iDir.y * deg2rad, iDir.x * deg2rad,
                                             iDir.z * deg2rad);
    }

    Vec3 transform(Vec3 const& pIn) const {
        Vec3 out = pIn * iScale;
        return iRotation * out;
    }
};

#pragma pack(push, 1)
struct WMOLiquidHeader {
    int32_t xverts{};
    int32_t yverts{};
    int32_t xtiles{};
    int32_t ytiles{};
    float   pos_x{};
    float   pos_y{};
    float   pos_z{};
    int16_t material{};
};
#pragma pack(pop)

} // namespace

struct GroupModel_Raw {
    uint32_t                 mogpflags{};
    uint32_t                 GroupWMOID{};
    AaBox3                   bounds{};
    uint32_t                 liquidflags{};
    std::vector<MeshTriangle> triangles;
    std::vector<Vec3>        vertexArray;
    WmoLiquid*               liquid{};

    ~GroupModel_Raw() { delete liquid; }

    bool Read(FILE* rf);
};

struct WorldModel_Raw {
    uint32_t                 RootWMOID{};
    std::vector<GroupModel_Raw> groupsArray;

    bool Read(char const* path);
};

// GroupModel_Raw::Read (ported from reference TileAssembler.cpp)
bool GroupModel_Raw::Read(FILE* rf) {
    char     blockId[5]{};
    blockId[4] = '\0';
    int32_t  blocksize{};

    if (std::fread(&mogpflags, sizeof(uint32_t), 1, rf) != 1) {
        return false;
    }
    if (std::fread(&GroupWMOID, sizeof(uint32_t), 1, rf) != 1) {
        return false;
    }

    Vec3 vec1{};
    Vec3 vec2{};
    if (std::fread(&vec1, sizeof(Vec3), 1, rf) != 1) {
        return false;
    }
    if (std::fread(&vec2, sizeof(Vec3), 1, rf) != 1) {
        return false;
    }
    bounds.set(vec1, vec2);

    if (std::fread(&liquidflags, sizeof(uint32_t), 1, rf) != 1) {
        return false;
    }

    uint32_t branches{};
    if (std::fread(blockId, 4, 1, rf) != 1) {
        return false;
    }
    if (std::strcmp(blockId, Firelands::VMap::kChunkGrp) != 0) {
        return false;
    }
    if (std::fread(&blocksize, sizeof(int32_t), 1, rf) != 1) {
        return false;
    }
    if (std::fread(&branches, sizeof(uint32_t), 1, rf) != 1) {
        return false;
    }
    for (uint32_t b = 0; b < branches; ++b) {
        uint32_t indexes{};
        if (std::fread(&indexes, sizeof(uint32_t), 1, rf) != 1) {
            return false;
        }
    }

    if (std::fread(blockId, 4, 1, rf) != 1) {
        return false;
    }
    if (std::strcmp(blockId, Firelands::VMap::kChunkIndx) != 0) {
        return false;
    }
    if (std::fread(&blocksize, sizeof(int32_t), 1, rf) != 1) {
        return false;
    }
    uint32_t nindexes{};
    if (std::fread(&nindexes, sizeof(uint32_t), 1, rf) != 1) {
        return false;
    }
    if (nindexes > 0) {
        std::vector<uint16_t> indexarray(nindexes);
        if (std::fread(indexarray.data(), sizeof(uint16_t), nindexes, rf) != nindexes) {
            return false;
        }
        triangles.reserve(nindexes / 3);
        for (uint32_t i = 0; i + 2 < nindexes; i += 3) {
            triangles.emplace_back(indexarray[i], indexarray[i + 1], indexarray[i + 2]);
        }
    }

    if (std::fread(blockId, 4, 1, rf) != 1) {
        return false;
    }
    if (std::strcmp(blockId, Firelands::VMap::kChunkVert) != 0) {
        return false;
    }
    if (std::fread(&blocksize, sizeof(int32_t), 1, rf) != 1) {
        return false;
    }
    uint32_t nvectors{};
    if (std::fread(&nvectors, sizeof(uint32_t), 1, rf) != 1) {
        return false;
    }
    if (nvectors > 0) {
        std::vector<float> vectorarray(static_cast<size_t>(nvectors) * 3u);
        if (std::fread(vectorarray.data(), sizeof(float), nvectors * 3u, rf) != nvectors * 3u) {
            return false;
        }
        vertexArray.reserve(nvectors);
        for (uint32_t i = 0; i < nvectors; ++i) {
            vertexArray.emplace_back(vectorarray[3 * i + 0], vectorarray[3 * i + 1],
                                       vectorarray[3 * i + 2]);
        }
    }

    liquid = nullptr;
    if (liquidflags & 3u) {
        if (std::fread(blockId, 4, 1, rf) != 1) {
            return false;
        }
        if (std::strcmp(blockId, Firelands::VMap::kChunkLiqu) != 0) {
            return false;
        }
        if (std::fread(&blocksize, sizeof(int32_t), 1, rf) != 1) {
            return false;
        }
        uint32_t liquidType{};
        if (std::fread(&liquidType, sizeof(uint32_t), 1, rf) != 1) {
            return false;
        }
        if (liquidflags & 1u) {
            WMOLiquidHeader hlq{};
            if (std::fread(&hlq, sizeof(WMOLiquidHeader), 1, rf) != 1) {
                return false;
            }
            liquid = new WmoLiquid(static_cast<uint32_t>(hlq.xtiles),
                                   static_cast<uint32_t>(hlq.ytiles),
                                   Vec3(hlq.pos_x, hlq.pos_y, hlq.pos_z), liquidType);
            uint32_t size = static_cast<uint32_t>(hlq.xverts * hlq.yverts);
            if (std::fread(liquid->GetHeightStorage(), sizeof(float), size, rf) != size) {
                return false;
            }
            size = static_cast<uint32_t>(hlq.xtiles * hlq.ytiles);
            if (std::fread(liquid->GetFlagsStorage(), sizeof(uint8_t), size, rf) != size) {
                return false;
            }
        } else {
            liquid = new WmoLiquid(0, 0, Vec3(), liquidType);
            liquid->GetHeightStorage()[0] = bounds.high().z;
        }
    }

    return true;
}

bool WorldModel_Raw::Read(char const* path) {
    FILE* rf = std::fopen(path, "rb");
    if (!rf) {
        std::printf("ERROR: Can't open raw model file: %s\n", path);
        return false;
    }

    char ident[9]{};
    ident[8] = '\0';
    if (std::fread(ident, 8, 1, rf) != 1) {
        std::fclose(rf);
        return false;
    }
    if (std::memcmp(ident, kRawVmapMagic, 8) != 0) {
        std::fclose(rf);
        return false;
    }

    uint32_t tempNVectors{};
    if (std::fread(&tempNVectors, sizeof(tempNVectors), 1, rf) != 1) {
        std::fclose(rf);
        return false;
    }

    uint32_t groups{};
    if (std::fread(&groups, sizeof(uint32_t), 1, rf) != 1) {
        std::fclose(rf);
        return false;
    }
    if (std::fread(&RootWMOID, sizeof(uint32_t), 1, rf) != 1) {
        std::fclose(rf);
        return false;
    }

    groupsArray.resize(groups);
    for (uint32_t g = 0; g < groups; ++g) {
        if (!groupsArray[g].Read(rf)) {
            std::fclose(rf);
            return false;
        }
    }

    std::fclose(rf);
    return true;
}

// ─── TileAssembler ───────────────────────────────────────────────────────────

TileAssembler::TileAssembler(std::string srcDir, std::string destDir)
    : iDestDir(std::move(destDir)), iSrcDir(std::move(srcDir)) {
    std::error_code ec;
    fs::create_directories(fs::path(iDestDir), ec);
}

TileAssembler::~TileAssembler() = default;

bool TileAssembler::convertWorld2() {
    if (!readMapSpawns()) {
        return false;
    }

    float constexpr invTileSize = 1.0f / kTileSize;

    bool success = true;
    while (!mapData.empty()) {
        MapSpawns data = std::move(mapData.front());
        mapData.pop_front();

        std::vector<ModelSpawn*> mapSpawns;
        mapSpawns.reserve(data.UniqueEntries.size());
        std::printf("Calculating model bounds for map %u...\n", data.MapId);

        for (auto entry = data.UniqueEntries.begin(); entry != data.UniqueEntries.end(); ++entry) {
            if (entry->second.flags & kModM2) {
                if (!calculateTransformedBound(entry->second)) {
                    continue;
                }
            }

            mapSpawns.push_back(&entry->second);
            spawnedModelFiles.insert(entry->second.name);

            std::map<uint32_t, std::set<TileSpawn>>& tileEntries =
                (entry->second.flags & kModParentSpawn) ? data.ParentTileEntries : data.TileEntries;

            AaBox3 const& bounds = entry->second.iBound;
            auto lowx  = static_cast<int16_t>(static_cast<int>(bounds.low().x * invTileSize));
            auto lowy  = static_cast<int16_t>(static_cast<int>(bounds.low().y * invTileSize));
            auto highx = static_cast<int16_t>(static_cast<int>(bounds.high().x * invTileSize));
            auto highy = static_cast<int16_t>(static_cast<int>(bounds.high().y * invTileSize));
            for (int x = lowx; x <= highx; ++x) {
                for (int y = lowy; y <= highy; ++y) {
                    tileEntries[PackTileId(static_cast<uint32_t>(x), static_cast<uint32_t>(y))]
                        .emplace(entry->second.ID, entry->second.flags);
                }
            }
        }

        std::printf("Creating map tree for map %u...\n", data.MapId);
        BIH pTree;
        try {
            auto getBounds = [](ModelSpawn* const& obj, AaBox3& out) { out = obj->iBound; };
            pTree.Build(mapSpawns, getBounds);
        } catch (std::exception const& e) {
            std::printf("Exception \"%s\" when calling BIH::Build\n", e.what());
            return false;
        }

        char mapPathC[1024]{};
        std::snprintf(mapPathC, sizeof(mapPathC), "%s/%03u.vmtree", iDestDir.c_str(), data.MapId);
        FILE* mapfile = std::fopen(mapPathC, "wb");
        if (!mapfile) {
            std::printf("Cannot open %s\n", mapPathC);
            success = false;
            break;
        }

        if (std::fwrite(kVmapMagic, 1, 8, mapfile) != 8) {
            success = false;
        }
        if (success && std::fwrite(kChunkNode, 1, 4, mapfile) != 4) {
            success = false;
        }
        if (success) {
            success = pTree.WriteToFile(mapfile);
        }

        uint32_t mapSpawnsSize = static_cast<uint32_t>(mapSpawns.size());
        if (success && std::fwrite(kChunkSidx, 1, 4, mapfile) != 4) {
            success = false;
        }
        if (success && std::fwrite(&mapSpawnsSize, sizeof(uint32_t), 1, mapfile) != 1) {
            success = false;
        }
        for (uint32_t i = 0; i < mapSpawnsSize && success; ++i) {
            if (std::fwrite(&mapSpawns[i]->ID, sizeof(uint32_t), 1, mapfile) != 1) {
                success = false;
            }
            if (std::fwrite(&i, sizeof(uint32_t), 1, mapfile) != 1) {
                success = false;
            }
        }
        std::fclose(mapfile);

        for (auto tileItr = data.TileEntries.begin(); tileItr != data.TileEntries.end(); ++tileItr) {
            uint32_t x = 0;
            uint32_t y = 0;
            UnpackTileId(tileItr->first, x, y);
            char tilePathC[1024]{};
            std::snprintf(tilePathC, sizeof(tilePathC), "%s/%03u_%02u_%02u.vmtile", iDestDir.c_str(),
                          data.MapId, y, x);
            FILE* tileFile = std::fopen(tilePathC, "wb");
            if (!tileFile) {
                continue;
            }

            std::set<TileSpawn> const& parentTileEntries = data.ParentTileEntries[tileItr->first];
            uint32_t                   nSpawns =
                static_cast<uint32_t>(tileItr->second.size() + parentTileEntries.size());

            if (success && std::fwrite(kVmapMagic, 1, 8, tileFile) != 8) {
                success = false;
            }
            if (success && std::fwrite(&nSpawns, sizeof(uint32_t), 1, tileFile) != 1) {
                success = false;
            }
            for (auto spawnItr = tileItr->second.begin();
                 spawnItr != tileItr->second.end() && success; ++spawnItr) {
                success = ModelSpawn::WriteToFile(tileFile, data.UniqueEntries.at(spawnItr->Id));
            }
            for (auto spawnItr = parentTileEntries.begin();
                 spawnItr != parentTileEntries.end() && success; ++spawnItr) {
                success = ModelSpawn::WriteToFile(tileFile, data.UniqueEntries.at(spawnItr->Id));
            }
            std::fclose(tileFile);
        }
    }

    exportGameobjectModels();

    std::printf("\nConverting Model Files\n");
    for (auto const& mfile : spawnedModelFiles) {
        std::printf("Converting %s\n", mfile.c_str());
        if (!convertRawFile(mfile)) {
            std::printf("error converting %s\n", mfile.c_str());
            success = false;
            break;
        }
    }
    return success;
}

bool TileAssembler::readMapSpawns() {
    std::string const fname = iSrcDir + "/dir_bin";
    FILE*             dirf  = std::fopen(fname.c_str(), "rb");
    if (!dirf) {
        std::printf("Could not read dir_bin file!\n");
        return false;
    }
    std::printf("Read coordinate mapping...\n");

    std::map<uint32_t, MapSpawns> data;
    while (!std::feof(dirf)) {
        uint32_t mapID = 0;
        size_t   check = std::fread(&mapID, sizeof(uint32_t), 1, dirf);
        if (check == 0) {
            break;
        }

        ModelSpawn spawn{};
        if (!ModelSpawn::ReadFromFile(dirf, spawn)) {
            break;
        }

        auto map_iter = data.emplace(std::piecewise_construct, std::forward_as_tuple(mapID),
                                     std::forward_as_tuple());
        if (map_iter.second) {
            map_iter.first->second.MapId = mapID;
            std::printf("spawning Map %u\n", mapID);
        }

        map_iter.first->second.UniqueEntries.emplace(spawn.ID, std::move(spawn));
    }

    mapData.resize(data.size());
    auto dst = mapData.begin();
    for (auto src = data.begin(); src != data.end(); ++src, ++dst) {
        *dst = std::move(src->second);
    }

    bool const success = (std::ferror(dirf) == 0);
    std::fclose(dirf);
    return success;
}

bool TileAssembler::calculateTransformedBound(ModelSpawn& spawn) {
    std::string modelFilename = iSrcDir;
    modelFilename.push_back('/');
    modelFilename.append(spawn.name);

    ModelPosition modelPosition{};
    modelPosition.iDir   = spawn.iRot;
    modelPosition.iScale = spawn.iScale;
    modelPosition.init();

    WorldModel_Raw raw_model{};
    if (!raw_model.Read(modelFilename.c_str())) {
        return false;
    }

    uint32_t groups = static_cast<uint32_t>(raw_model.groupsArray.size());
    if (groups == 0) {
        return false;
    }
    if (groups != 1) {
        std::printf("Warning: '%s' does not seem to be a M2 model!\n", modelFilename.c_str());
    }

    AaBox3 rotated_bounds{};
    bool   first = true;
    for (int i = 0; i < 8; ++i) {
        Vec3 const c  = BoxCorner(raw_model.groupsArray[0].bounds, i);
        Vec3 const t  = modelPosition.transform(c);
        if (first) {
            rotated_bounds.lo = rotated_bounds.hi = t;
            first               = false;
        } else {
            rotated_bounds.merge(t);
        }
    }

    spawn.iBound = rotated_bounds;
    spawn.iBound.lo += spawn.iPos;
    spawn.iBound.hi += spawn.iPos;
    spawn.flags = static_cast<uint8_t>(spawn.flags | kModHasBound);
    return true;
}

bool TileAssembler::convertRawFile(std::string const& pModelFilename) {
    std::string filename = iSrcDir;
    if (!filename.empty()) {
        filename.push_back('/');
    }
    filename.append(pModelFilename);

    WorldModel_Raw raw_model{};
    if (!raw_model.Read(filename.c_str())) {
        return false;
    }

    WorldModel model{};
    model.setRootWmoID(raw_model.RootWMOID);
    if (!raw_model.groupsArray.empty()) {
        std::vector<GroupModel> groupsArray;
        uint32_t                gcount = static_cast<uint32_t>(raw_model.groupsArray.size());
        groupsArray.reserve(gcount);
        for (uint32_t g = 0; g < gcount; ++g) {
            GroupModel_Raw& raw_group = raw_model.groupsArray[g];
            groupsArray.emplace_back(raw_group.mogpflags, raw_group.GroupWMOID, raw_group.bounds);
            std::vector<Vec3>         verts = std::move(raw_group.vertexArray);
            std::vector<MeshTriangle> tris  = std::move(raw_group.triangles);
            groupsArray.back().setMeshData(verts, tris);
            groupsArray.back().setLiquidData(raw_group.liquid);
            raw_group.liquid = nullptr;
        }
        model.setGroupModels(groupsArray);
    }

    fs::path outPath = fs::path(iDestDir) / (pModelFilename + ".vmo");
    std::error_code ec;
    fs::create_directories(outPath.parent_path(), ec);
    return model.writeFile(outPath.string());
}

void TileAssembler::exportGameobjectModels() {
    std::string const listPath = iSrcDir + "/temp_gameobject_models";
    FILE*             model_list = std::fopen(listPath.c_str(), "rb");
    if (!model_list) {
        return;
    }

    char ident[8]{};
    if (std::fread(ident, 1, 8, model_list) != 8 || std::memcmp(ident, kRawVmapMagic, 8) != 0) {
        std::fclose(model_list);
        return;
    }

    std::string const outList = iDestDir + "/" + std::string(kGameObjectModels);
    FILE*             model_list_copy = std::fopen(outList.c_str(), "wb");
    if (!model_list_copy) {
        std::fclose(model_list);
        return;
    }

    std::fwrite(kVmapMagic, 1, 8, model_list_copy);

    uint32_t name_length{};
    uint32_t displayId{};
    uint8_t  isWmo{};
    char      buff[500]{};

    while (true) {
        if (std::fread(&displayId, sizeof(uint32_t), 1, model_list) != 1) {
            if (std::feof(model_list)) {
                break;
            }
            break;
        }
        if (std::fread(&isWmo, sizeof(uint8_t), 1, model_list) != 1 ||
            std::fread(&name_length, sizeof(uint32_t), 1, model_list) != 1 ||
            name_length >= sizeof(buff) ||
            std::fread(buff, sizeof(char), name_length, model_list) != name_length) {
            std::printf("\nFile 'temp_gameobject_models' seems to be corrupted\n");
            break;
        }

        std::string const model_name(buff, name_length);

        WorldModel_Raw raw_model{};
        if (!raw_model.Read((iSrcDir + "/" + model_name).c_str())) {
            continue;
        }

        spawnedModelFiles.insert(model_name);

        AaBox3 bounds{};
        bool   boundEmpty = true;
        for (uint32_t g = 0; g < raw_model.groupsArray.size(); ++g) {
            std::vector<Vec3>& vertices = raw_model.groupsArray[g].vertexArray;
            for (uint32_t i = 0; i < vertices.size(); ++i) {
                Vec3& v = vertices[i];
                if (boundEmpty) {
                    bounds.lo = bounds.hi = v;
                    boundEmpty             = false;
                } else {
                    bounds.merge(v);
                }
            }
        }

        if (boundEmpty) {
            std::printf("\nModel %s has empty bounding box\n", model_name.c_str());
            continue;
        }

        std::fwrite(&displayId, sizeof(uint32_t), 1, model_list_copy);
        std::fwrite(&isWmo, sizeof(uint8_t), 1, model_list_copy);
        std::fwrite(&name_length, sizeof(uint32_t), 1, model_list_copy);
        std::fwrite(buff, sizeof(char), name_length, model_list_copy);
        std::fwrite(&bounds.lo, sizeof(Vec3), 1, model_list_copy);
        std::fwrite(&bounds.hi, sizeof(Vec3), 1, model_list_copy);
    }

    std::fclose(model_list);
    std::fclose(model_list_copy);
}

} // namespace Firelands::VMap::Assembler
