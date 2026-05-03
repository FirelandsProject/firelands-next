#include "MapExtractorTask.h"

#include "../common/MpqStream.h"
#include "../common/DbcReader.h"
#include "../common/VMapMagic.h"
#include "WdtReader.h"

#include "StormLib.h"

#include <bitset>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
namespace Firelands::VMap::MapExtractor {

// ─── Deep-water suppression list (matches reference IsDeepWaterIgnored) ──────

static bool IsDeepWaterIgnored(uint32_t mapId, int y, int x) {
    if (mapId == 0)  // Azeroth — Vashj'ir grids
        return (x >= 39 && x <= 40 && y >= 24 && y <= 26) ||
               (x >= 41 && x <= 46 && y >= 18 && y <= 26);
    if (mapId == 1)  // Kalimdor — Thousand Needles
        return x == 43 && (y == 39 || y == 40);
    return false;
}

// ─── MPQ opening helpers ─────────────────────────────────────────────────────

static constexpr const char* kWorldMpqs[] = {
    "world.MPQ", "art.MPQ", "world2.MPQ",
    "expansion1.MPQ", "expansion2.MPQ", "expansion3.MPQ",
};

static constexpr const char* kLocales[] = {
    "enGB","enUS","deDE","esES","frFR","koKR",
    "zhCN","zhTW","enCN","enTW","esMX","ruRU","ptBR","ptPT","itIT"
};

static constexpr uint32_t kBuilds[] = {
    13164,13205,13287,13329,13596,13623,13914,14007,14333,14480,
    14545,15005,15050,15211,15354,15595,0
};

// Open the world MPQ chain (world.MPQ + art + expansion patches).
static HANDLE OpenWorldMpq(const fs::path& dataDir, uint32_t targetBuild) {
    HANDLE h = nullptr;
    std::string base = (dataDir / "world.MPQ").string();
    if (!SFileOpenArchive(base.c_str(), 0, MPQ_OPEN_READ_ONLY, &h))
        return nullptr;

    int count = static_cast<int>(sizeof(kWorldMpqs) / sizeof(kWorldMpqs[0]));
    for (int i = 1; i < count; ++i) {
        if (targetBuild < kNewBaseSetBuild && std::string(kWorldMpqs[i]) == "world2.MPQ")
            continue;
        std::string p = (dataDir / kWorldMpqs[i]).string();
        SFileOpenPatchArchive(h, p.c_str(), "", 0);
    }
    for (int i = 0; kBuilds[i] && kBuilds[i] <= targetBuild; ++i) {
        if (targetBuild >= kNewBaseSetBuild && kBuilds[i] < kNewBaseSetBuild)
            continue;
        std::string p;
        const char* prefix;
        if (kBuilds[i] > kLastDbcInDataBuild) {
            prefix = "";
            p = (dataDir / ("wow-update-base-" + std::to_string(kBuilds[i]) + ".MPQ")).string();
        } else {
            prefix = "base";
            p = (dataDir / ("wow-update-" + std::to_string(kBuilds[i]) + ".MPQ")).string();
        }
        SFileOpenPatchArchive(h, p.c_str(), prefix, 0);
    }
    return h;
}

// Open the first available locale MPQ + its patches.
static HANDLE OpenLocaleMpq(const fs::path& dataDir, uint32_t targetBuild,
                             std::string& outLocale) {
    for (const char* loc : kLocales) {
        std::string archivePath = (dataDir / loc / (std::string("locale-") + loc + ".MPQ")).string();
        HANDLE h = nullptr;
        if (!SFileOpenArchive(archivePath.c_str(), 0, MPQ_OPEN_READ_ONLY, &h))
            continue;

        for (int i = 0; kBuilds[i] && kBuilds[i] <= targetBuild; ++i) {
            if (targetBuild >= kNewBaseSetBuild && kBuilds[i] < kNewBaseSetBuild)
                continue;
            std::string p;
            const char* prefix;
            if (kBuilds[i] > kLastDbcInDataBuild) {
                prefix = "";
                p = (dataDir / loc / (std::string("wow-update-") + loc + "-" + std::to_string(kBuilds[i]) + ".MPQ")).string();
            } else {
                prefix = loc;
                p = (dataDir / ("wow-update-" + std::to_string(kBuilds[i]) + ".MPQ")).string();
            }
            SFileOpenPatchArchive(h, p.c_str(), prefix, 0);
        }
        outLocale = loc;
        return h;
    }
    return nullptr;
}

// ─── DBC loading ─────────────────────────────────────────────────────────────

struct MapEntry { uint32_t id; char name[64]; };

static std::vector<MapEntry> ReadMapDbc(HANDLE locale, bool verbose) {
    MpqStream s(locale, "DBFilesClient\\Map.dbc");
    std::vector<MapEntry> result;
    if (!s.IsValid()) {
        if (verbose) std::fprintf(stderr, "[map] Cannot open Map.dbc\n");
        return result;
    }
    DbcReader dbc(locale, "DBFilesClient\\Map.dbc");
    if (!dbc.IsOpen()) return result;
    result.resize(dbc.RecordCount());
    for (uint32_t i = 0; i < dbc.RecordCount(); ++i) {
        result[i].id = dbc.GetUInt(i, 0);
        std::strncpy(result[i].name, dbc.GetString(i, 1), 63);
        result[i].name[63] = '\0';
    }
    if (verbose) std::printf("Map.dbc: %u maps loaded\n", static_cast<uint32_t>(result.size()));
    return result;
}

static void ReadLiquidDbcs(HANDLE locale, LiquidDbcTables& dbc, bool verbose) {
    // LiquidMaterial.dbc
    {
        DbcReader r(locale, "DBFilesClient\\LiquidMaterial.dbc");
        if (r.IsOpen()) {
            for (uint32_t i = 0; i < r.RecordCount(); ++i)
                dbc.materials[r.GetUInt(i,0)] = { static_cast<int8_t>(r.GetUInt(i,1)) };
            if (verbose) std::printf("LiquidMaterial.dbc: %u entries\n", r.RecordCount());
        }
    }
    // LiquidObject.dbc
    {
        DbcReader r(locale, "DBFilesClient\\LiquidObject.dbc");
        if (r.IsOpen()) {
            for (uint32_t i = 0; i < r.RecordCount(); ++i)
                dbc.objects[r.GetUInt(i,0)] = { static_cast<int16_t>(r.GetUInt(i,3)) };
            if (verbose) std::printf("LiquidObject.dbc:   %u entries\n", r.RecordCount());
        }
    }
    // LiquidType.dbc (fields: id=0, SoundBank=3, MaterialID=14)
    {
        DbcReader r(locale, "DBFilesClient\\LiquidType.dbc");
        if (r.IsOpen()) {
            for (uint32_t i = 0; i < r.RecordCount(); ++i)
                dbc.types[r.GetUInt(i,0)] = {
                    static_cast<uint8_t>(r.GetUInt(i, 3)),
                    static_cast<uint8_t>(r.GetUInt(i, 14))
                };
            if (verbose) std::printf("LiquidType.dbc:     %u entries\n", r.RecordCount());
        }
    }
}

// ─── Cinematic camera M2 extract (CinematicCamera.dbc) ───────────────────────
// Matches reference map_extractor: list paths from DBC (locale MPQ), open each
// M2 from the world MPQ patch chain, write under outputRoot/Cameras/<relative>.

static std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static std::filesystem::path RelativeUnderCamerasDir(std::string mpqInternalPath) {
    for (char& c : mpqInternalPath) {
        if (c == '\\') {
            c = '/';
        }
    }
    std::string const lower = ToLowerAscii(mpqInternalPath);
    static constexpr char kPrefix[] = "cameras/";
    std::size_t const pos = lower.find(kPrefix);
    if (pos == std::string::npos) {
        return {};
    }
    return std::filesystem::path(mpqInternalPath.substr(pos + sizeof(kPrefix) - 1));
}

static bool WriteMpqFileToDisk(HANDLE worldMpq, char const* internalPath,
                               std::filesystem::path const& destPath) {
    HANDLE file = nullptr;
    // Same as MpqStream: opened archive already has patch chain; FROM_MPQ resolves patched file.
    if (!SFileOpenFileEx(worldMpq, internalPath, SFILE_OPEN_FROM_MPQ, &file)) {
        return false;
    }
    DWORD hi = 0;
    DWORD lo = SFileGetFileSize(file, &hi);
    if (hi != 0 || lo == 0) {
        SFileCloseFile(file);
        return false;
    }
    std::vector<uint8_t> buf(lo);
    DWORD read = 0;
    if (!SFileReadFile(file, buf.data(), lo, &read, nullptr) || read != lo) {
        SFileCloseFile(file);
        return false;
    }
    SFileCloseFile(file);

    std::error_code ec;
    fs::create_directories(destPath.parent_path(), ec);
    FILE* out = std::fopen(destPath.string().c_str(), "wb");
    if (!out) {
        return false;
    }
    std::size_t const n = std::fwrite(buf.data(), 1, buf.size(), out);
    std::fclose(out);
    return n == buf.size();
}

// Returns number of camera M2 files written, or 0 if DBC missing / no paths.
static int ExtractCameraM2Files(HANDLE worldMpq, HANDLE localeMpq,
                                 fs::path const& outputRoot, bool verbose) {
    if (verbose) {
        std::printf("Extracting cinematic camera M2 files...\n");
    }

    DbcReader camdbc(localeMpq, "DBFilesClient\\CinematicCamera.dbc");
    if (!camdbc.IsOpen()) {
        if (verbose) {
            std::fprintf(stderr,
                         "[map] CinematicCamera.dbc not found — camera extract skipped.\n");
        }
        return 0;
    }

    std::unordered_set<std::string> seen;
    std::vector<std::string> mpqPaths;
    mpqPaths.reserve(camdbc.RecordCount());

    for (uint32_t i = 0; i < camdbc.RecordCount(); ++i) {
        std::string camFile(camdbc.GetString(i, 1));
        if (camFile.empty()) {
            continue;
        }
        std::size_t const loc = camFile.find(".mdx");
        if (loc != std::string::npos) {
            camFile.replace(loc, 4, ".m2");
        }
        if (seen.insert(camFile).second) {
            mpqPaths.push_back(std::move(camFile));
        }
    }

    fs::path const camerasRoot = outputRoot / "Cameras";
    int count = 0;

    for (std::string const& internal : mpqPaths) {
        fs::path const rel = RelativeUnderCamerasDir(internal);
        if (rel.empty() || rel.string().find("..") != std::string::npos) {
            if (verbose) {
                std::fprintf(stderr, "[map] Skipping camera path (unexpected layout): %s\n",
                             internal.c_str());
            }
            continue;
        }
        fs::path const dest = camerasRoot / rel;
        if (fs::exists(dest)) {
            continue;
        }
        if (!WriteMpqFileToDisk(worldMpq, internal.c_str(), dest)) {
            if (verbose) {
                std::fprintf(stderr, "[map] Unable to extract camera file: %s\n",
                             internal.c_str());
            }
            continue;
        }
        ++count;
    }

    if (verbose) {
        std::printf("Extracted %d cinematic camera file(s) under %s\n", count,
                    camerasRoot.string().c_str());
    }
    return count;
}

// ─── RunMapExtractorTask ──────────────────────────────────────────────────────

int RunMapExtractorTask(const MapExtractorOptions& opts) {
    // Open MPQs
    std::string locale;
    HANDLE localeMpq = OpenLocaleMpq(opts.dataDir, opts.build, locale);
    if (!localeMpq) {
        std::fprintf(stderr, "[map] No locale MPQ found in %s\n",
                     opts.dataDir.string().c_str());
        return -1;
    }
    if (opts.verbose) std::printf("Locale: %s\n", locale.c_str());

    HANDLE worldMpq = OpenWorldMpq(opts.dataDir, opts.build);
    if (!worldMpq) {
        std::fprintf(stderr, "[map] Cannot open world.MPQ in %s\n",
                     opts.dataDir.string().c_str());
        SFileCloseArchive(localeMpq);
        return -1;
    }

    // Load DBC tables
    LiquidDbcTables dbcTables;
    ReadLiquidDbcs(localeMpq, dbcTables, opts.verbose);

    // Read map list
    auto maps = ReadMapDbc(localeMpq, opts.verbose);
    if (maps.empty()) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return -1;
    }

    // Create output directory
    fs::path mapsDir = opts.outputDir / "maps";
    fs::create_directories(mapsDir);

    int totalWritten = 0;

    for (auto& map : maps) {
        if (opts.verbose)
            std::printf("Processing map: %-32s (id=%u)\n", map.name, map.id);

        // Load WDT
        char wdtPath[256];
        std::snprintf(wdtPath, sizeof(wdtPath),
                      "World\\Maps\\%s\\%s.wdt", map.name, map.name);
        MpqStream wdtStream(worldMpq, wdtPath, false);
        if (!wdtStream.IsValid()) continue;

        WdtTileGrid tileGrid;
        if (!WdtReader::Parse(wdtStream.Data(), wdtStream.Size(), tileGrid))
            continue;

        std::bitset<kWdtMapSize * kWdtMapSize> convertedTiles;

        for (int y = 0; y < kWdtMapSize; ++y) {
            for (int x = 0; x < kWdtMapSize; ++x) {
                if (!tileGrid.TileExists(y, x)) continue;

                // Load ADT
                char adtPath[256];
                std::snprintf(adtPath, sizeof(adtPath),
                              "World\\Maps\\%s\\%s_%d_%d.adt",
                              map.name, map.name, x, y);
                MpqStream adtStream(worldMpq, adtPath, false);
                if (!adtStream.IsValid()) continue;

                bool ignoreDeepWater = IsDeepWaterIgnored(map.id, y, x);
                AdtGridData grid;
                if (!AdtReader::Parse(adtStream.Data(), adtStream.Size(),
                                      dbcTables, ignoreDeepWater, grid))
                    continue;

                // Build output path: <mapsDir>/<mapId><y><x>.map
                char outName[32];
                std::snprintf(outName, sizeof(outName), "%03u%02u%02u.map", map.id, y, x);
                bool ok = MapFileWriter::Write(grid, mapsDir / outName,
                                               opts.build, opts.writeOpts);
                if (ok) { convertedTiles.set(y * kWdtMapSize + x); ++totalWritten; }
            }
            if (opts.verbose) {
                std::printf("\r  %3d%%", (100 * (y + 1)) / kWdtMapSize);
                std::fflush(stdout);
            }
        }
        if (opts.verbose) std::printf("\n");

        // Write .tilelist
        char tlName[32];
        std::snprintf(tlName, sizeof(tlName), "%03u.tilelist", map.id);
        MapFileWriter::WriteTileList(mapsDir / tlName, convertedTiles, opts.build);
    }

    ExtractCameraM2Files(worldMpq, localeMpq, opts.outputDir, opts.verbose);

    SFileCloseArchive(worldMpq);
    SFileCloseArchive(localeMpq);

    if (opts.verbose)
        std::printf("\nDone. Wrote %d .map file(s) to %s\n",
                    totalWritten, mapsDir.string().c_str());
    return totalWritten;
}

int ExtractSingleServerMapTile(const MapExtractorOptions& opts, uint32_t mapId, int tileY,
                               int tileX, const fs::path& outMapPath) {
    std::string locale;
    HANDLE localeMpq = OpenLocaleMpq(opts.dataDir, opts.build, locale);
    if (!localeMpq) {
        return -1;
    }

    HANDLE worldMpq = OpenWorldMpq(opts.dataDir, opts.build);
    if (!worldMpq) {
        SFileCloseArchive(localeMpq);
        return -1;
    }

    LiquidDbcTables dbcTables;
    ReadLiquidDbcs(localeMpq, dbcTables, false);

    auto maps = ReadMapDbc(localeMpq, false);
    if (maps.empty()) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return -1;
    }

    MapEntry const* chosen = nullptr;
    for (auto const& m : maps) {
        if (m.id == mapId) {
            chosen = &m;
            break;
        }
    }
    if (!chosen) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return -1;
    }

    char wdtPath[256];
    std::snprintf(wdtPath, sizeof(wdtPath), "World\\Maps\\%s\\%s.wdt", chosen->name, chosen->name);
    MpqStream wdtStream(worldMpq, wdtPath, false);
    if (!wdtStream.IsValid()) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return -1;
    }

    WdtTileGrid tileGrid;
    if (!WdtReader::Parse(wdtStream.Data(), wdtStream.Size(), tileGrid)) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return -1;
    }

    if (!tileGrid.TileExists(tileY, tileX)) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return 0;
    }

    char adtPath[256];
    std::snprintf(adtPath, sizeof(adtPath), "World\\Maps\\%s\\%s_%d_%d.adt", chosen->name,
                  chosen->name, tileX, tileY);
    MpqStream adtStream(worldMpq, adtPath, false);
    if (!adtStream.IsValid()) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return 0;
    }

    bool const            ignoreDeepWater = IsDeepWaterIgnored(mapId, tileY, tileX);
    AdtGridData           grid;
    if (!AdtReader::Parse(adtStream.Data(), adtStream.Size(), dbcTables, ignoreDeepWater, grid)) {
        SFileCloseArchive(worldMpq);
        SFileCloseArchive(localeMpq);
        return 0;
    }

    bool const ok =
        MapFileWriter::Write(grid, outMapPath, opts.build, opts.writeOpts);
    SFileCloseArchive(worldMpq);
    SFileCloseArchive(localeMpq);
    return ok ? 1 : -1;
}

} // namespace Firelands::VMap::MapExtractor
