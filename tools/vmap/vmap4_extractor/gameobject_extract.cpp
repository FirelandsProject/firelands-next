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

#include "model.h"
#include "adtfile.h"
#include "vmapexport.h"

#include "../common/DbcReader.h"
#include "../common/VMapMagic.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

bool ExtractSingleModel(std::string& fname)
    {
    if (fname.length() < 4)
        return false;

    std::string extension = fname.substr(fname.length() - 4, 4);
    if (extension == ".mdx" || extension == ".MDX" || extension == ".mdl" || extension == ".MDL")
    {
        fname.erase(fname.length() - 2, 2);
        fname.append("2");
    }

    std::string originalName = fname;

    char* name = GetPlainName((char*)fname.c_str());
    FixNameCase(name, strlen(name));
    FixNameSpaces(name, strlen(name));

    std::string output = g_workDirWmo;
    output += "/";
    output += name;

    if (FileExists(output.c_str()))
        return true;

    Model mdl(originalName);
    if (!mdl.open())
        return false;

    return mdl.ConvertToVMAPModel(output.c_str());
    }

extern HANDLE LocaleMpq;

void ExtractGameobjectModels()
    {
    if (!g_vmap4Quiet)
        printf("Extracting GameObject models...");
    Firelands::VMap::DbcReader dbc(LocaleMpq, "DBFilesClient\\GameObjectDisplayInfo.dbc");
    if (!dbc.IsOpen())
    {
        printf("Fatal error: Invalid GameObjectDisplayInfo.dbc file format!\n");
        exit(1);
    }

    std::string basepath = g_workDirWmo;
    basepath += "/";
    std::string path;

    std::string modelListPath = basepath + "temp_gameobject_models";
    FILE* model_list = fopen(modelListPath.c_str(), "wb");
    if (!model_list)
    {
        printf("Fatal error: Could not open file %s\n", modelListPath.c_str());
        return;
    }

    fwrite(Firelands::VMap::kRawVmapMagic, 1, 8, model_list);

    for (uint32 row = 0; row < dbc.RecordCount(); ++row)
    {
        path = dbc.GetString(row, 1);

        if (path.length() < 4)
            continue;

        FixNameCase((char*)path.c_str(), path.size());
        char * name = GetPlainName((char*)path.c_str());
    FixNameSpaces(name, strlen(name));

        char * ch_ext = GetExtension(name);
        if (!ch_ext)
            continue;

        bool result = false;
        uint8 isWmo = 0;
        if (!strcmp(ch_ext, ".wmo"))
    {
            isWmo = 1;
            result = ExtractSingleWmo(path);
    }
        else if (!strcmp(ch_ext, ".mdl"))   // TODO: extract .mdl files, if needed
            continue;
        else //if (!strcmp(ch_ext, ".mdx") || !strcmp(ch_ext, ".m2"))
            result = ExtractSingleModel(path);

        if (result)
    {
            uint32 displayId = dbc.GetUInt(row, 0);
            uint32 path_length = strlen(name);
            fwrite(&displayId, sizeof(uint32), 1, model_list);
            fwrite(&isWmo, sizeof(uint8), 1, model_list);
            fwrite(&path_length, sizeof(uint32), 1, model_list);
            fwrite(name, sizeof(char), path_length, model_list);
    }
    }

    fclose(model_list);

    if (!g_vmap4Quiet)
        printf("Done!\n");
    }
