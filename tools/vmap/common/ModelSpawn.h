#pragma once

// ModelSpawn — the per-instance record written into the tile directory files
// by vmap4_extractor and read by vmap4_assembler.
//
// Binary layout (matches reference implementation ModelInstance.cpp exactly):
//   uint8   flags
//   uint8   adtId
//   uint32  ID
//   float[3] iPos
//   float[3] iRot
//   float   iScale
//   — only when (flags & MOD_HAS_BOUND) —
//   float[3] iBound.lo
//   float[3] iBound.hi
//   uint32  nameLen
//   char[nameLen]  name  (no null terminator written)

#include "Vec3.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace Firelands::VMap {

enum ModelFlags : uint8_t {
    kModM2         = 0x01,
    kModHasBound   = 0x02,
    kModParentSpawn = 0x04,
};

struct ModelSpawn {
    uint8_t  flags{};
    uint8_t  adtId{};
    uint32_t ID{};
    Vec3     iPos{};
    Vec3     iRot{};
    float    iScale{1.f};
    AaBox3   iBound{};
    std::string name;

    bool HasBound()   const { return (flags & kModHasBound)   != 0; }
    bool IsM2()       const { return (flags & kModM2)         != 0; }
    bool IsParent()   const { return (flags & kModParentSpawn) != 0; }

    // Read from binary file.  Returns false on EOF or corruption.
    static bool ReadFromFile(FILE* rf, ModelSpawn& spawn);

    // Write to binary file.  Returns false on I/O error.
    static bool WriteToFile(FILE* wf, const ModelSpawn& spawn);
};

} // namespace Firelands::VMap
