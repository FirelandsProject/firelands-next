#pragma once

#include "../common/ModelSpawn.h"

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Firelands::VMap::Assembler {

struct TileSpawn {
    TileSpawn() : Id(0), Flags(0) {}
    TileSpawn(uint32_t id, uint32_t flags) : Id(id), Flags(flags) {}

    uint32_t Id{};
    uint32_t Flags{};

    bool operator<(TileSpawn const& right) const { return Id < right.Id; }
};

struct MapSpawns {
    uint32_t MapId{};
    std::map<uint32_t, Firelands::VMap::ModelSpawn> UniqueEntries;
    std::map<uint32_t, std::set<TileSpawn>>          TileEntries;
    std::map<uint32_t, std::set<TileSpawn>>          ParentTileEntries;
};

using MapData = std::deque<MapSpawns>;

/// Converts vmap4_extractor output (`Buildings/` with `dir_bin` and raw models)
/// into runtime `vmaps/` layout. Ported from reference `TileAssembler`.
class TileAssembler {
public:
    TileAssembler(std::string srcDir, std::string destDir);
    ~TileAssembler();

    TileAssembler(TileAssembler const&)            = delete;
    TileAssembler& operator=(TileAssembler const&) = delete;

    bool convertWorld2();

private:
    bool readMapSpawns();
    bool calculateTransformedBound(Firelands::VMap::ModelSpawn& spawn);
    void exportGameobjectModels();
    bool convertRawFile(std::string const& modelFilename);

    std::string              iDestDir;
    std::string              iSrcDir;
    MapData                  mapData;
    std::set<std::string>    spawnedModelFiles;
};

} // namespace Firelands::VMap::Assembler
