#include "WorldModelWrite.h"

#include "../common/VMapMagic.h"

#include <algorithm>
#include <cstring>

namespace Firelands::VMap::Assembler {

using namespace Firelands::VMap;

// ─── WmoLiquid ───────────────────────────────────────────────────────────────

WmoLiquid::WmoLiquid(uint32_t width, uint32_t height, Vec3 const& corner, uint32_t type)
    : iTilesX(width), iTilesY(height), iCorner(corner), iType(type) {
    if (width && height) {
        iHeight = new float[(width + 1) * (height + 1)];
        iFlags  = new uint8_t[width * height];
    } else {
        iHeight = new float[1];
        iFlags  = nullptr;
    }
}

WmoLiquid::WmoLiquid(WmoLiquid const& other) : iHeight(nullptr), iFlags(nullptr) {
    *this = other;
}

WmoLiquid::~WmoLiquid() {
    delete[] iHeight;
    delete[] iFlags;
}

WmoLiquid& WmoLiquid::operator=(WmoLiquid const& other) {
    if (this == &other) {
        return *this;
    }
    iTilesX = other.iTilesX;
    iTilesY = other.iTilesY;
    iCorner = other.iCorner;
    iType   = other.iType;
    delete[] iHeight;
    delete[] iFlags;
    if (other.iHeight) {
        iHeight = new float[(iTilesX + 1) * (iTilesY + 1)];
        std::memcpy(iHeight, other.iHeight,
                    (iTilesX + 1) * (iTilesY + 1) * sizeof(float));
    } else {
        iHeight = nullptr;
    }
    if (other.iFlags) {
        iFlags = new uint8_t[iTilesX * iTilesY];
        std::memcpy(iFlags, other.iFlags, iTilesX * iTilesY);
    } else {
        iFlags = nullptr;
    }
    return *this;
}

uint32_t WmoLiquid::GetFileSize() const {
    return static_cast<uint32_t>(
        2 * sizeof(uint32_t) + sizeof(Vec3) + sizeof(uint32_t) +
        (iFlags ? ((iTilesX + 1) * (iTilesY + 1) * sizeof(float) + iTilesX * iTilesY)
                : sizeof(float)));
}

bool WmoLiquid::writeToFile(FILE* wf) {
    bool result = false;
    if (std::fwrite(&iTilesX, sizeof(uint32_t), 1, wf) == 1 &&
        std::fwrite(&iTilesY, sizeof(uint32_t), 1, wf) == 1 &&
        std::fwrite(&iCorner, sizeof(Vec3), 1, wf) == 1 &&
        std::fwrite(&iType, sizeof(uint32_t), 1, wf) == 1) {
        if (iTilesX && iTilesY) {
            uint32_t size = (iTilesX + 1) * (iTilesY + 1);
            if (std::fwrite(iHeight, sizeof(float), size, wf) == size) {
                size = iTilesX * iTilesY;
                result = std::fwrite(iFlags, sizeof(uint8_t), size, wf) == size;
            }
        } else {
            result = std::fwrite(iHeight, sizeof(float), 1, wf) == 1;
        }
    }
    return result;
}

// ─── GroupModel ──────────────────────────────────────────────────────────────

GroupModel::GroupModel(uint32_t mogpFlags, uint32_t groupWMOID, AaBox3 const& bound)
    : iBound(bound), iMogpFlags(mogpFlags), iGroupWMOID(groupWMOID), iLiquid(nullptr) {}

GroupModel::GroupModel(GroupModel const& other)
    : iBound(other.iBound), iMogpFlags(other.iMogpFlags), iGroupWMOID(other.iGroupWMOID),
      vertices(other.vertices), triangles(other.triangles), meshTree(other.meshTree),
      iLiquid(nullptr) {
    if (other.iLiquid) {
        iLiquid = new WmoLiquid(*other.iLiquid);
    }
}

GroupModel::~GroupModel() {
    delete iLiquid;
}

void GroupModel::setMeshData(std::vector<Vec3>& vert, std::vector<MeshTriangle>& tri) {
    vertices.swap(vert);
    triangles.swap(tri);
    auto getBounds = [this](MeshTriangle const& t, AaBox3& out) {
        Vec3 lo = vertices[t.idx0];
        Vec3 hi = lo;
        for (uint32_t ix : {t.idx1, t.idx2}) {
            Vec3 const& v = vertices[ix];
            lo.x = std::min(lo.x, v.x);
            lo.y = std::min(lo.y, v.y);
            lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x);
            hi.y = std::max(hi.y, v.y);
            hi.z = std::max(hi.z, v.z);
        }
        out.lo = lo;
        out.hi = hi;
    };
    meshTree.Build(triangles, getBounds);
}

bool GroupModel::writeToFile(FILE* wf) const {
    bool       result = true;
    uint32_t   chunkSize{};
    uint32_t   count{};

    if (result && std::fwrite(&iBound, sizeof(AaBox3), 1, wf) != 1) {
        result = false;
    }
    if (result && std::fwrite(&iMogpFlags, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }
    if (result && std::fwrite(&iGroupWMOID, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }

    if (result && std::fwrite(kChunkVert, 1, 4, wf) != 4) {
        result = false;
    }
    count     = static_cast<uint32_t>(vertices.size());
    chunkSize = static_cast<uint32_t>(sizeof(uint32_t) + sizeof(Vec3) * count);
    if (result && std::fwrite(&chunkSize, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }
    if (result && std::fwrite(&count, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }
    if (!count) {
        return result;
    }
    if (result && std::fwrite(vertices.data(), sizeof(Vec3), count, wf) != count) {
        result = false;
    }

    if (result && std::fwrite(kChunkTrim, 1, 4, wf) != 4) {
        result = false;
    }
    count     = static_cast<uint32_t>(triangles.size());
    chunkSize = static_cast<uint32_t>(sizeof(uint32_t) + sizeof(MeshTriangle) * count);
    if (result && std::fwrite(&chunkSize, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }
    if (result && std::fwrite(&count, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }
    if (result && std::fwrite(triangles.data(), sizeof(MeshTriangle), count, wf) != count) {
        result = false;
    }

    if (result && std::fwrite(kChunkMbih, 1, 4, wf) != 4) {
        result = false;
    }
    if (result) {
        result = meshTree.WriteToFile(wf);
    }

    if (result && std::fwrite(kChunkLiqu, 1, 4, wf) != 4) {
        result = false;
    }
    if (!iLiquid) {
        chunkSize = 0;
        if (result && std::fwrite(&chunkSize, sizeof(uint32_t), 1, wf) != 1) {
            result = false;
        }
        return result;
    }
    chunkSize = iLiquid->GetFileSize();
    if (result && std::fwrite(&chunkSize, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }
    if (result) {
        result = iLiquid->writeToFile(wf);
    }
    return result;
}

// ─── WorldModel ──────────────────────────────────────────────────────────────

void WorldModel::setGroupModels(std::vector<GroupModel>& models) {
    groupModels.swap(models);
    auto getBounds = [](GroupModel const& g, AaBox3& out) { out = g.GetBound(); };
    groupTree.Build(groupModels, getBounds, 1);
}

bool WorldModel::writeFile(std::string const& filename) const {
    FILE* wf = std::fopen(filename.c_str(), "wb");
    if (!wf) {
        return false;
    }

    uint32_t chunkSize{};
    uint32_t count{};
    bool     result = std::fwrite(kVmapMagic, 1, 8, wf) == 8;
    if (result && std::fwrite(kChunkWmod, 1, 4, wf) != 4) {
        result = false;
    }
    chunkSize = static_cast<uint32_t>(sizeof(uint32_t) + sizeof(uint32_t));
    if (result && std::fwrite(&chunkSize, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }
    if (result && std::fwrite(&RootWMOID, sizeof(uint32_t), 1, wf) != 1) {
        result = false;
    }

    count = static_cast<uint32_t>(groupModels.size());
    if (count) {
        if (result && std::fwrite(kChunkGmod, 1, 4, wf) != 4) {
            result = false;
        }
        if (result && std::fwrite(&count, sizeof(uint32_t), 1, wf) != 1) {
            result = false;
        }
        for (uint32_t i = 0; i < count && result; ++i) {
            result = groupModels[i].writeToFile(wf);
        }
        if (result && std::fwrite(kChunkGbih, 1, 4, wf) != 4) {
            result = false;
        }
        if (result) {
            result = groupTree.WriteToFile(wf);
        }
    }

    std::fclose(wf);
    return result;
}

} // namespace Firelands::VMap::Assembler
