#pragma once

// WorldModel / GroupModel / WmoLiquid — write-only subset for vmap4_assembler.
// Ported from reference WorldModel.* (G3D replaced with Vec3 / AaBox3).

#include "../common/BoundingIntervalHierarchy.h"
#include "../common/Vec3.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace Firelands::VMap::Assembler {

struct MeshTriangle {
    uint32_t idx0{};
    uint32_t idx1{};
    uint32_t idx2{};

    MeshTriangle() = default;
    MeshTriangle(uint32_t a, uint32_t b, uint32_t c) : idx0(a), idx1(b), idx2(c) {}
};

class WmoLiquid {
public:
    WmoLiquid(uint32_t width, uint32_t height, Vec3 const& corner, uint32_t type);
    WmoLiquid(WmoLiquid const& other);
    ~WmoLiquid();
    WmoLiquid& operator=(WmoLiquid const& other);

    float*       GetHeightStorage() { return iHeight; }
    uint8_t*     GetFlagsStorage() { return iFlags; }
    uint32_t     GetType() const { return iType; }
    uint32_t     GetFileSize() const;
    bool         writeToFile(FILE* wf);

private:
    WmoLiquid() = default;

    uint32_t iTilesX{};
    uint32_t iTilesY{};
    Vec3     iCorner{};
    uint32_t iType{};
    float*   iHeight{};
    uint8_t* iFlags{};
};

class GroupModel {
public:
    GroupModel() = default;
    GroupModel(GroupModel const& other);
    GroupModel(uint32_t mogpFlags, uint32_t groupWMOID, AaBox3 const& bound);
    ~GroupModel();

    void setMeshData(std::vector<Vec3>& vert, std::vector<MeshTriangle>& tri);
    void setLiquidData(WmoLiquid*& liquid) {
        delete iLiquid;
        iLiquid = liquid;
        liquid = nullptr;
    }

    bool     writeToFile(FILE* wf) const;
    AaBox3 const& GetBound() const { return iBound; }
    uint32_t GetMogpFlags() const { return iMogpFlags; }
    uint32_t GetWmoID() const { return iGroupWMOID; }

private:
    AaBox3                      iBound{};
    uint32_t                    iMogpFlags{};
    uint32_t                    iGroupWMOID{};
    std::vector<Vec3>         vertices;
    std::vector<MeshTriangle> triangles;
    BIH                         meshTree;
    WmoLiquid*                  iLiquid{};
};

class WorldModel {
public:
    WorldModel() = default;

    void setGroupModels(std::vector<GroupModel>& models);
    void setRootWmoID(uint32_t id) { RootWMOID = id; }
    void SetFlags(uint32_t f) { Flags = f; }

    bool writeFile(std::string const& filename) const;

private:
    uint32_t              Flags{};
    uint32_t              RootWMOID{};
    std::vector<GroupModel> groupModels;
    BIH                   groupTree;
};

} // namespace Firelands::VMap::Assembler
