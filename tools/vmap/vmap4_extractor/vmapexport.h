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

#pragma once

#include "vmap4_types.h"

#include <string>
#include <unordered_map>

struct WMODoodadData;

enum ModelFlags
{
    MOD_M2 = 1,
    MOD_HAS_BOUND = 1 << 1,
    MOD_PARENT_SPAWN = 1 << 2
};

// Output directory for converted WMO/M2 and dir/dir_bin (no trailing separator).
extern std::string g_workDirWmo;
extern std::unordered_map<std::string, WMODoodadData> WmoDoodads;
uint32 GenerateUniqueObjectId(uint32 clientId, uint16 clientDoodadId);

bool FileExists(const char * file);

bool ExtractSingleWmo(std::string& fname);
bool ExtractSingleModel(std::string& fname);

void ExtractGameobjectModels();

// When true, suppresses non-essential progress and MPQ chatter (see `-q`).
extern bool g_vmap4Quiet;
