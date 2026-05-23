/*
  * This file is part of the Firelands project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "adtfile.h"
#include "Vmap4ExtractorCli.h"
#include "wdtfile.h"
#include "wmo.h"
#include "vmapexport.h"

#include "../common/DbcReader.h"
#include "../common/VMapMagic.h"

#include <StormLib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

#if !defined(_WIN32)
#include <unistd.h>
#ifndef ERROR_PATH_NOT_FOUND
#define ERROR_PATH_NOT_FOUND ERROR_FILE_NOT_FOUND
#endif
#endif

#undef min
#undef max

#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

//------------------------------------------------------------------------------
// Defines

#define MPQ_BLOCK_SIZE 0x1000

//-----------------------------------------------------------------------------

HANDLE WorldMpq = nullptr;
HANDLE LocaleMpq = nullptr;

uint32 CONF_TargetBuild = Firelands::VMap::kTargetBuild;

// List MPQ for extract maps from
char const* CONF_mpq_list[]=
{
    "world.MPQ",
    "art.MPQ",
    "expansion1.MPQ",
    "expansion2.MPQ",
    "expansion3.MPQ",
    "world2.MPQ",
};

uint32 const Builds[] = {13164, 13205, 13287, 13329, 13596, 13623, 13914, 14007, 14333, 14480, 14545, 15005, 15050, 15211, 15354, 15595, 0};
#define LAST_DBC_IN_DATA_BUILD 13623    // after this build mpqs with dbc are back to locale folder
#define NEW_BASE_SET_BUILD  15211

#define LOCALES_COUNT 15

char const* Locales[LOCALES_COUNT] =
{
    "enGB", "enUS",
    "deDE", "esES",
    "frFR", "koKR",
    "zhCN", "zhTW",
    "enCN", "enTW",
    "esMX", "ruRU",
    "ptBR", "ptPT",
    "itIT"
};

struct map_info
{
    char name[64];
    int32 parent_id;
};

struct LiquidMaterialEntry
{
    int8 LVF;
};

struct LiquidObjectEntry
{
    int16 LiquidTypeID;
};

struct LiquidTypeEntry
{
    uint8 SoundBank;
    uint8 MaterialID;
};

std::map<uint32, map_info> map_ids;
std::unordered_set<uint32> maps_that_are_parents;
std::string g_inputDataPath;
bool preciseVectorData = false;
std::unordered_map<std::string, WMODoodadData> WmoDoodads;

std::string g_workDirWmo;

bool g_vmap4Quiet = false;

static std::string PathWithTrailingSep(fs::path p) {
    p = p.lexically_normal();
    std::string s = p.string();
    if (!s.empty() && s.back() != '/' && s.back() != '\\')
        s.push_back(static_cast<char>(fs::path::preferred_separator));
    return s;
}

bool LoadLocaleMPQFile(int locale)
{
    char buff[4096];
    memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "%s%s/locale-%s.MPQ", g_inputDataPath.c_str(), Locales[locale], Locales[locale]);
    if (!SFileOpenArchive(buff, 0, MPQ_OPEN_READ_ONLY, &LocaleMpq))
    {
        if (GetLastError() != ERROR_PATH_NOT_FOUND)
        {
            if (!g_vmap4Quiet) {
                printf("Loading %s locale MPQs\n", Locales[locale]);
            }
            printf("Cannot open archive %s\n", buff);
        }
        return false;
    }

    if (!g_vmap4Quiet)
        printf("Loading %s locale MPQs\n", Locales[locale]);
    char const* prefix = nullptr;
    for (int i = 0; Builds[i] && Builds[i] <= CONF_TargetBuild; ++i)
    {
        // Do not attempt to read older MPQ patch archives past this build, they were merged with base
        // and trying to read them together with new base will not end well
        if (CONF_TargetBuild >= NEW_BASE_SET_BUILD && Builds[i] < NEW_BASE_SET_BUILD)
            continue;

        memset(buff, 0, sizeof(buff));
        if (Builds[i] > LAST_DBC_IN_DATA_BUILD)
        {
            prefix = "";
            std::snprintf(buff, sizeof(buff), "%s%s/wow-update-%s-%u.MPQ", g_inputDataPath.c_str(), Locales[locale],
                          Locales[locale], Builds[i]);
        }
        else
        {
            prefix = Locales[locale];
            std::snprintf(buff, sizeof(buff), "%swow-update-%u.MPQ", g_inputDataPath.c_str(), Builds[i]);
        }

        if (!SFileOpenPatchArchive(LocaleMpq, buff, prefix, 0))
        {
            if (GetLastError() != ERROR_FILE_NOT_FOUND)
                printf("Cannot open patch archive %s\n", buff);
            continue;
        }
    }

    if (!g_vmap4Quiet)
        printf("\n");
    return true;
}

void LoadCommonMPQFiles(uint32 build)
{
    char filename[4096];
    std::snprintf(filename, sizeof(filename), "%sworld.MPQ", g_inputDataPath.c_str());
    if (!g_vmap4Quiet)
        printf("Loading common MPQ files\n");
    if (!SFileOpenArchive(filename, 0, MPQ_OPEN_READ_ONLY, &WorldMpq))
    {
        if (GetLastError() != ERROR_PATH_NOT_FOUND)
            printf("Cannot open archive %s\n", filename);
        return;
    }

    int count = sizeof(CONF_mpq_list) / sizeof(char*);
    for (int i = 1; i < count; ++i)
    {
        if (build < 15211 && !strcmp("world2.MPQ", CONF_mpq_list[i]))   // 4.3.2 and higher MPQ
            continue;

        std::snprintf(filename, sizeof(filename), "%s%s", g_inputDataPath.c_str(), CONF_mpq_list[i]);
        if (!SFileOpenPatchArchive(WorldMpq, filename, "", 0))
        {
            if (GetLastError() != ERROR_PATH_NOT_FOUND)
                printf("Cannot open archive %s\n", filename);
            else
                printf("Not found %s\n", filename);
        }
        else if (!g_vmap4Quiet)
            printf("Loaded %s\n", filename);
    }

    char const* prefix = nullptr;
    for (int i = 0; Builds[i] && Builds[i] <= CONF_TargetBuild; ++i)
    {
        // Do not attempt to read older MPQ patch archives past this build, they were merged with base
        // and trying to read them together with new base will not end well
        if (CONF_TargetBuild >= NEW_BASE_SET_BUILD && Builds[i] < NEW_BASE_SET_BUILD)
            continue;

        memset(filename, 0, sizeof(filename));
        if (Builds[i] > LAST_DBC_IN_DATA_BUILD)
        {
            prefix = "";
            std::snprintf(filename, sizeof(filename), "%swow-update-base-%u.MPQ", g_inputDataPath.c_str(), Builds[i]);
        }
        else
        {
            prefix = "base";
            std::snprintf(filename, sizeof(filename), "%swow-update-%u.MPQ", g_inputDataPath.c_str(), Builds[i]);
        }

        if (!SFileOpenPatchArchive(WorldMpq, filename, prefix, 0))
        {
            if (GetLastError() != ERROR_PATH_NOT_FOUND)
                printf("Cannot open patch archive %s\n", filename);
            else
                printf("Not found %s\n", filename);
            continue;
        }
        else if (!g_vmap4Quiet)
            printf("Loaded %s\n", filename);
    }

    if (!g_vmap4Quiet)
        printf("\n");
}

std::map<std::pair<uint32, uint16>, uint32> uniqueObjectIds;

uint32 GenerateUniqueObjectId(uint32 clientId, uint16 clientDoodadId)
{
    return uniqueObjectIds.emplace(std::make_pair(clientId, clientDoodadId), uniqueObjectIds.size() + 1).first->second;
}
// Local testing functions

bool FileExists(char const* file)
{
    if (FILE* n = fopen(file, "rb"))
    {
        fclose(n);
        return true;
    }
    return false;
}

bool ExtractWmo()
{
    bool success = false;

    //char const* ParsArchiveNames[] = {"patch-2.MPQ", "patch.MPQ", "common.MPQ", "expansion.MPQ"};

    SFILE_FIND_DATA data;
    HANDLE find = SFileFindFirstFile(WorldMpq, "*.wmo", &data, nullptr);
    if (find != nullptr)
    {
        do
        {
            std::string str = data.cFileName;
            //printf("Extracting wmo %s\n", str.c_str());
            success |= ExtractSingleWmo(str);
        }
        while (SFileFindNextFile(find, &data));
    }
    SFileFindClose(find);

    if (success && !g_vmap4Quiet)
        printf("\nExtract wmo complete (No (fatal) errors)\n");

    return success;
}

bool ExtractSingleWmo(std::string& fname)
{
    // Copy files from archive
    std::string originalName = fname;

    char szLocalFile[1024];
    char* plain_name = GetPlainName(&fname[0]);
    FixNameCase(plain_name, strlen(plain_name));
    FixNameSpaces(plain_name, strlen(plain_name));
    std::snprintf(szLocalFile, sizeof(szLocalFile), "%s/%s", g_workDirWmo.c_str(), plain_name);

    if (FileExists(szLocalFile))
        return true;

    int p = 0;
    // Select root wmo files
    char const* rchr = strrchr(plain_name, '_');
    if (rchr != nullptr)
    {
        char cpy[4];
        memcpy(cpy, rchr, 4);
        for (int i = 0; i < 4; ++i)
        {
            int m = cpy[i];
            if (isdigit(m))
                p++;
        }
    }

    if (p == 3)
        return true;

    bool file_ok = true;
    WMORoot froot(originalName);
    if (!froot.open())
    {
        printf("Couldn't open RootWmo!\n");
        return true;
    }
    FILE *output = fopen(szLocalFile,"wb");
    if (!output)
    {
        printf("Couldn't open %s for writing!\n", szLocalFile);
        return false;
    }
    froot.ConvertToVMAPRootWmo(output);
    WMODoodadData& doodads = WmoDoodads[plain_name];
    std::swap(doodads, froot.DoodadData);
    int Wmo_nVertices = 0;
    uint32 groupCount = 0;
    //printf("root has %d groups\n", froot->nGroups);
    if (froot.nGroups !=0)
    {
        for (uint32 i = 0; i < froot.nGroups; ++i)
        {
            char temp[1024];
            strncpy(temp, fname.c_str(), 1024);
            temp[fname.length()-4] = 0;
            char groupFileName[1024];
            std::snprintf(groupFileName, sizeof(groupFileName), "%s_%03u.wmo", temp, i);
            //printf("Trying to open groupfile %s\n",groupFileName);

            std::string s = groupFileName;
            WMOGroup fgroup(s);
            if (!fgroup.open(&froot))
            {
                printf("Could not open all Group file for: %s\n", plain_name);
                file_ok = false;
                break;
            }

            if (fgroup.ShouldSkip(&froot))
                continue;

            Wmo_nVertices += fgroup.ConvertToVMAPGroupWmo(output, preciseVectorData);
            ++groupCount;
            for (uint16 groupReference : fgroup.DoodadReferences)
            {
                if (groupReference >= doodads.Spawns.size())
                    continue;

                uint32 doodadNameIndex = doodads.Spawns[groupReference].NameIndex;
                if (froot.ValidDoodadNames.find(doodadNameIndex) == froot.ValidDoodadNames.end())
                    continue;

                doodads.References.insert(groupReference);
            }
        }
    }

    fseek(output, 8, SEEK_SET); // store the correct no of vertices
    fwrite(&Wmo_nVertices, sizeof(int), 1, output);
    fwrite(&groupCount, sizeof(uint32), 1, output);
    fclose(output);

    // Delete the extracted file in the case of an error
    if (!file_ok)
        remove(szLocalFile);
    return true;
}

void ParsMapFiles()
{
    std::unordered_map<uint32, WDTFile> wdts;
    auto getWDT = [&wdts](uint32 mapId) -> WDTFile*
    {
        auto itr = wdts.find(mapId);
        if (itr == wdts.end())
        {
            char fn[512];
            char* name = map_ids[mapId].name;
            std::snprintf(fn, sizeof(fn), "World\\Maps\\%s\\%s.wdt", name, name);
            itr = wdts.emplace(std::piecewise_construct, std::forward_as_tuple(mapId), std::forward_as_tuple(fn, name, maps_that_are_parents.count(mapId) > 0)).first;
            if (!itr->second.init(mapId))
            {
                wdts.erase(itr);
                return nullptr;
            }
        }

        return &itr->second;
    };

    for (auto itr = map_ids.begin(); itr != map_ids.end(); ++itr)
    {
        if (WDTFile* WDT = getWDT(itr->first))
        {
            WDTFile* parentWDT = itr->second.parent_id >= 0 ? getWDT(itr->second.parent_id) : nullptr;
            if (!g_vmap4Quiet)
                printf("Processing Map %u\n[", itr->first);
            for (int32 x = 0; x < 64; ++x)
            {
                for (int32 y = 0; y < 64; ++y)
                {
                    bool success = false;
                    if (ADTFile* ADT = WDT->GetMap(x, y))
                    {
                        success = ADT->init(itr->first, itr->first);
                        WDT->FreeADT(ADT);
                    }
                    if (!success && parentWDT)
                    {
                        if (ADTFile* ADT = parentWDT->GetMap(x, y))
                        {
                            ADT->init(itr->first, itr->second.parent_id);
                            parentWDT->FreeADT(ADT);
                        }
                    }
                }
                if (!g_vmap4Quiet) {
                    printf("#");
                    fflush(stdout);
                }
            }
            if (!g_vmap4Quiet)
                printf("]\n");
        }
    }
}

static bool ApplyPathsFromWowAndOutput(fs::path const& wowInstall, fs::path const& outputRoot,
                                       std::string& err) {
    fs::path dataDir = wowInstall / "Data";
    if (!fs::is_directory(dataDir)) {
        err = "Not a WoW install directory (missing Data/): " + dataDir.string();
        return false;
    }
    g_inputDataPath = PathWithTrailingSep(dataDir);
    g_workDirWmo    = (outputRoot / "Buildings").lexically_normal().string();
    return true;
}

int main(int argc, char** argv) {
    const char* const versionString = "V4.05 2018_03";

    fs::path wowInstall;
    fs::path outputRoot;

    if (argc == 1) {
        std::printf("Firelands VMAP4 extractor (buildings + dir)\n\n");
        std::printf("WoW install directory (folder containing Data): ");
        std::string wowLine;
        std::getline(std::cin, wowLine);
        if (wowLine.empty()) {
            std::printf("Cancelled.\n");
            return 0;
        }
        std::printf("Output root directory: ");
        std::string outLine;
        std::getline(std::cin, outLine);
        if (outLine.empty()) {
            std::printf("Cancelled.\n");
            return 0;
        }
        wowInstall  = fs::path(wowLine);
        outputRoot  = fs::path(outLine);
        preciseVectorData = false;
        g_vmap4Quiet        = false;
        CONF_TargetBuild    = Firelands::VMap::kTargetBuild;
    } else {
        using Firelands::VMap::Vmap4Extractor::CliOptions;
        using Firelands::VMap::Vmap4Extractor::CliParseResult;
        using Firelands::VMap::Vmap4Extractor::ParseCli;
        using Firelands::VMap::Vmap4Extractor::PrintUsage;

        CliOptions cli{};
        CliParseResult const pr = ParseCli(argc, argv, cli);
        if (pr == CliParseResult::Help) {
            PrintUsage(argv[0]);
            return 0;
        }
        if (pr != CliParseResult::Ok) {
            std::fprintf(stderr, "Invalid arguments.\n\n");
            PrintUsage(argv[0]);
            return 1;
        }
        wowInstall          = cli.wowInstall;
        outputRoot          = cli.outputRoot;
        CONF_TargetBuild    = cli.build;
        preciseVectorData   = cli.preciseLargePayload;
        g_vmap4Quiet        = cli.quiet;
    }

    std::string pathErr;
    if (!ApplyPathsFromWowAndOutput(wowInstall, outputRoot, pathErr)) {
        std::fprintf(stderr, "%s\n", pathErr.c_str());
        return 1;
    }

    std::string sdir     = g_workDirWmo + "/dir";
    std::string sdir_bin = g_workDirWmo + "/dir_bin";
    struct stat status {};
    if (stat(sdir.c_str(), &status) == 0 || stat(sdir_bin.c_str(), &status) == 0) {
        std::fprintf(stderr,
                     "Output Buildings directory already contains dir/ or dir_bin/.\n"
                     "Use an empty output root (or delete those folders) before extracting.\n");
        return 1;
    }

    if (!g_vmap4Quiet) {
        std::printf("Firelands VMAP4 extractor — %s\n", versionString);
        std::printf("Data:      %s\n", g_inputDataPath.c_str());
        std::printf("Buildings: %s\n\n", g_workDirWmo.c_str());
        std::printf("Beginning work...\n\n");
    }

    std::error_code mkEc;
    fs::create_directories(fs::path(g_workDirWmo), mkEc);
    if (mkEc) {
        std::fprintf(stderr, "Cannot create Buildings directory: %s\n", mkEc.message().c_str());
        return 1;
    }

    bool success = true;

    LoadCommonMPQFiles(CONF_TargetBuild);

    bool localeOk = false;
    for (int i = 0; i < LOCALES_COUNT; ++i) {
        if (!LoadLocaleMPQFile(i)) {
            if (GetLastError() != ERROR_PATH_NOT_FOUND)
                printf("Unable to load %s locale archives!\n", Locales[i]);
            continue;
        }
        if (!g_vmap4Quiet)
            printf("Detected and using locale: %s\n", Locales[i]);
        localeOk = true;
        break;
    }
    if (!localeOk) {
        std::fprintf(stderr, "FATAL: Could not open any locale MPQ under Data/.\n");
        SFileCloseArchive(WorldMpq);
        return 1;
    }

    ExtractGameobjectModels();

    if (success) {
        if (!g_vmap4Quiet)
            printf("Read Map.dbc file... ");

        Firelands::VMap::DbcReader dbc(LocaleMpq, "DBFilesClient\\Map.dbc");
        if (!dbc.IsOpen()) {
            printf("FATAL ERROR: Map.dbc not found in data file.\n");
            SFileCloseArchive(LocaleMpq);
            SFileCloseArchive(WorldMpq);
            return 1;
        }

        for (uint32 x = 0; x < dbc.RecordCount(); ++x) {
            map_info& m = map_ids[dbc.GetUInt(x, 0)];

            char const* map_name = dbc.GetString(x, 1);
            size_t max_map_name_length = sizeof(m.name);
            if (strlen(map_name) >= max_map_name_length) {
                printf("Fatal error: Map name too long!\n");
                SFileCloseArchive(LocaleMpq);
                SFileCloseArchive(WorldMpq);
                return 1;
            }

            strncpy(m.name, map_name, max_map_name_length);
            m.name[max_map_name_length - 1] = '\0';
            m.parent_id = static_cast<int32>(dbc.GetInt(x, 19));
            if (m.parent_id >= 0)
                maps_that_are_parents.insert(m.parent_id);

            if (!g_vmap4Quiet)
                printf("Map - %s\n", m.name);
        }

        ParsMapFiles();
    }

    SFileCloseArchive(LocaleMpq);
    SFileCloseArchive(WorldMpq);

    if (!g_vmap4Quiet)
        printf("\n");
    if (!success) {
        std::fprintf(stderr, "ERROR: Extract %s. Work NOT complete.\n", versionString);
        return 1;
    }

    if (!g_vmap4Quiet)
        printf("Extract %s. Work complete. No errors.\n", versionString);
    return 0;
}
