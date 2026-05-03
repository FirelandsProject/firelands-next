#include "ModelSpawn.h"

#include <cstdio>
#include <cstring>
#include <iostream>

namespace Firelands::VMap {

// ─── ReadFromFile ─────────────────────────────────────────────────────────────
// Mirrors ModelSpawn::readFromFile in reference implementation ModelInstance.cpp
// exactly, replacing G3D::Vector3 / G3D::AABox with our Vec3 / AaBox3.

bool ModelSpawn::ReadFromFile(FILE* rf, ModelSpawn& spawn) {
    uint32_t check = 0;

    // flags — first byte; also serves as EOF sentinel.
    check += static_cast<uint32_t>(std::fread(&spawn.flags, sizeof(uint8_t), 1, rf));
    if (!check) {
        if (std::ferror(rf))
            std::cerr << "ModelSpawn::ReadFromFile: I/O error reading flags\n";
        return false;
    }

    check += static_cast<uint32_t>(std::fread(&spawn.adtId,  sizeof(uint8_t),  1, rf));
    check += static_cast<uint32_t>(std::fread(&spawn.ID,     sizeof(uint32_t), 1, rf));
    check += static_cast<uint32_t>(std::fread(&spawn.iPos.x, sizeof(float),    3, rf));
    check += static_cast<uint32_t>(std::fread(&spawn.iRot.x, sizeof(float),    3, rf));
    check += static_cast<uint32_t>(std::fread(&spawn.iScale, sizeof(float),    1, rf));

    bool hasBound = spawn.HasBound();
    if (hasBound) {
        check += static_cast<uint32_t>(std::fread(&spawn.iBound.lo.x, sizeof(float), 3, rf));
        check += static_cast<uint32_t>(std::fread(&spawn.iBound.hi.x, sizeof(float), 3, rf));
    }

    uint32_t nameLen = 0;
    check += static_cast<uint32_t>(std::fread(&nameLen, sizeof(uint32_t), 1, rf));

    uint32_t expected = hasBound ? 17u : 11u;
    if (check != expected) {
        std::cerr << "ModelSpawn::ReadFromFile: unexpected field count "
                  << check << " (expected " << expected << ")\n";
        return false;
    }

    if (nameLen > 500) {
        std::cerr << "ModelSpawn::ReadFromFile: name too long (" << nameLen << ")\n";
        return false;
    }

    char nameBuf[501] = {};
    uint32_t nr = static_cast<uint32_t>(std::fread(nameBuf, sizeof(char), nameLen, rf));
    if (nr != nameLen) {
        std::cerr << "ModelSpawn::ReadFromFile: name read error\n";
        return false;
    }
    spawn.name.assign(nameBuf, nameLen);
    return true;
}

// ─── WriteToFile ─────────────────────────────────────────────────────────────
// Mirrors ModelSpawn::writeToFile in reference implementation ModelInstance.cpp.

bool ModelSpawn::WriteToFile(FILE* wf, const ModelSpawn& spawn) {
    uint32_t check = 0;

    check += static_cast<uint32_t>(std::fwrite(&spawn.flags,  sizeof(uint8_t),  1, wf));
    check += static_cast<uint32_t>(std::fwrite(&spawn.adtId,  sizeof(uint8_t),  1, wf));
    check += static_cast<uint32_t>(std::fwrite(&spawn.ID,     sizeof(uint32_t), 1, wf));
    check += static_cast<uint32_t>(std::fwrite(&spawn.iPos.x, sizeof(float),    3, wf));
    check += static_cast<uint32_t>(std::fwrite(&spawn.iRot.x, sizeof(float),    3, wf));
    check += static_cast<uint32_t>(std::fwrite(&spawn.iScale, sizeof(float),    1, wf));

    bool hasBound = spawn.HasBound();
    if (hasBound) {
        check += static_cast<uint32_t>(std::fwrite(&spawn.iBound.lo.x, sizeof(float), 3, wf));
        check += static_cast<uint32_t>(std::fwrite(&spawn.iBound.hi.x, sizeof(float), 3, wf));
    }

    uint32_t nameLen = static_cast<uint32_t>(spawn.name.size());
    check += static_cast<uint32_t>(std::fwrite(&nameLen, sizeof(uint32_t), 1, wf));

    uint32_t expected = hasBound ? 17u : 11u;
    if (check != expected) return false;

    uint32_t nw = static_cast<uint32_t>(
        std::fwrite(spawn.name.c_str(), sizeof(char), nameLen, wf));
    return nw == nameLen;
}

} // namespace Firelands::VMap
