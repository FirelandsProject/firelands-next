#include "AdtReader.h"

#include <algorithm>
#include <cstring>

namespace Firelands::VMap::MapExtractor {

// ─── helpers ─────────────────────────────────────────────────────────────────

// Interpret 4 bytes at ptr as a raw uint32.
static inline uint32_t Read32(const uint8_t* p) {
    uint32_t v; std::memcpy(&v, p, 4); return v;
}
static inline float ReadF32(const uint8_t* p) {
    float v; std::memcpy(&v, p, 4); return v;
}

// FOURCC comparisons — tags in WoW files are stored reversed on disk.
// We compare the raw (reversed) value stored in the file.
// e.g.  "MVER" is stored as bytes { 'R','E','V','M' } = 0x4D524556
static constexpr uint32_t RawTag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(d) << 24u) |
           (static_cast<uint32_t>(c) << 16u) |
           (static_cast<uint32_t>(b) <<  8u) |
           (static_cast<uint32_t>(a));
}

static constexpr uint32_t kTagMVER = RawTag('R','E','V','M');
static constexpr uint32_t kTagMCNK = RawTag('K','N','C','M');
static constexpr uint32_t kTagMCVT = RawTag('T','V','C','M');
static constexpr uint32_t kTagMCLQ = RawTag('Q','L','C','M');
static constexpr uint32_t kTagMH2O = RawTag('O','2','H','M');
static constexpr uint32_t kTagMFBO = RawTag('O','B','F','M');
template <typename T>
static const T* ResolveMcnkSubChunk(const uint8_t* mcnkData,
                                    uint32_t mcnkSize,
                                    uint32_t offset,
                                    uint32_t expectedTag) {
    if (offset == 0)
        return nullptr;

    uint32_t const candidates[] = {8u + offset, offset};
    uint32_t const totalSize = 8u + mcnkSize;
    for (uint32_t candidate : candidates) {
        if (candidate + sizeof(T) > totalSize)
            continue;

        const T* chunk = reinterpret_cast<const T*>(mcnkData + candidate);
        if (chunk->fcc == expectedTag)
            return chunk;
    }

    return nullptr;
}

// ─── LiquidVertexFormat resolution ──────────────────────────────────────────

LiquidVertexFormat AdtReader::GetLiquidVertexFormat(
    const uint8_t* /*mh2oBase*/,
    const adt_liquid_instance* h,
    const LiquidDbcTables& dbc)
{
    if (h->LiquidVertexFormat < 42)
        return static_cast<LiquidVertexFormat>(h->LiquidVertexFormat);

    if (h->LiquidType == 2)
        return LiquidVertexFormat::Depth;

    auto it = dbc.types.find(h->LiquidType);
    if (it != dbc.types.end()) {
        auto im = dbc.materials.find(it->second.MaterialID);
        if (im != dbc.materials.end())
            return static_cast<LiquidVertexFormat>(im->second.LVF);
    }
    return LiquidVertexFormat::Invalid;
}

// ─── MH2O height accessor ────────────────────────────────────────────────────

float AdtReader::GetMh2oLiquidHeight(const uint8_t* mh2oBase,
                                     const adt_liquid_instance* h,
                                     LiquidVertexFormat fmt, int pos)
{
    if (!h->OffsetVertexData) return 0.f;
    const uint8_t* vd = mh2oBase + h->OffsetVertexData;
    int w1 = h->GetEffectiveWidth()  + 1;
    int h1 = h->GetEffectiveHeight() + 1;

    switch (fmt) {
    case LiquidVertexFormat::HeightDepth:
    case LiquidVertexFormat::HeightTextureCoord:
    case LiquidVertexFormat::HeightDepthTextureCoord:
        return ReadF32(vd + pos * 4);
    case LiquidVertexFormat::Depth:
        return 0.f;
    case LiquidVertexFormat::Unk4:
    case LiquidVertexFormat::Unk5:
        return ReadF32(vd + 4 + pos * 8);
    default:
        break;
    }
    (void)w1; (void)h1;
    return 0.f;
}

// ─── ProcessMcnk ─────────────────────────────────────────────────────────────

void AdtReader::ProcessMcnk(const uint8_t* mcnkData,
                             bool ignoreDeepWater,
                             AdtGridData& out)
{
    const adt_MCNK* mcnk = reinterpret_cast<const adt_MCNK*>(mcnkData);

    int iy = static_cast<int>(mcnk->iy);
    int ix = static_cast<int>(mcnk->ix);
    if (iy < 0 || iy >= kAdtCellsPerGrid ||
        ix < 0 || ix >= kAdtCellsPerGrid) return;

    // Area IDs
    out.area_ids[iy][ix] = static_cast<uint16_t>(mcnk->areaid);

    // Seed V9 / V8 from MCNK base height (ypos in WoW = Z-up)
    float baseH = mcnk->ypos;
    for (int y = 0; y <= kAdtCellSize; ++y) {
        int cy = iy * kAdtCellSize + y;
        for (int x = 0; x <= kAdtCellSize; ++x) {
            int cx = ix * kAdtCellSize + x;
            out.V9[cy][cx] = baseH;
        }
    }
    for (int y = 0; y < kAdtCellSize; ++y) {
        int cy = iy * kAdtCellSize + y;
        for (int x = 0; x < kAdtCellSize; ++x) {
            int cx = ix * kAdtCellSize + x;
            out.V8[cy][cx] = baseH;
        }
    }

    // MCVT sub-chunk (relative offsets within the MCNK payload)
    if (mcnk->offsMCVT) {
        const adt_MCVT* mcvt =
            ResolveMcnkSubChunk<adt_MCVT>(mcnkData, mcnk->size,
                                          mcnk->offsMCVT, kTagMCVT);
        if (mcvt) {
        // V9: outer grid (ADT_CELL_SIZE+1 × ADT_CELL_SIZE+1), stride = 2*CELL_SIZE+1
        for (int y = 0; y <= kAdtCellSize; ++y) {
            int cy = iy * kAdtCellSize + y;
            for (int x = 0; x <= kAdtCellSize; ++x) {
                int cx = ix * kAdtCellSize + x;
                out.V9[cy][cx] += mcvt->height_map[y * (kAdtCellSize * 2 + 1) + x];
            }
        }
        // V8: inner grid, offset by CELL_SIZE+1 in the height_map array
        for (int y = 0; y < kAdtCellSize; ++y) {
            int cy = iy * kAdtCellSize + y;
            for (int x = 0; x < kAdtCellSize; ++x) {
                int cx = ix * kAdtCellSize + x;
                out.V8[cy][cx] += mcvt->height_map[y * (kAdtCellSize * 2 + 1) + kAdtCellSize + 1 + x];
            }
        }
        }
    }

    // MCLQ legacy liquid
    if (mcnk->sizeMCLQ > 8 && mcnk->offsMCLQ) {
        const adt_MCLQ* liq =
            ResolveMcnkSubChunk<adt_MCLQ>(mcnkData, mcnk->size,
                                          mcnk->offsMCLQ, kTagMCLQ);
        if (!liq)
            return;
        int count = 0;
        for (int y = 0; y < kAdtCellSize; ++y) {
            int cy = iy * kAdtCellSize + y;
            for (int x = 0; x < kAdtCellSize; ++x) {
                int cx = ix * kAdtCellSize + x;
                if (liq->flags[y][x] != kMclqHiddenFlag) {
                    out.liquid_show[cy][cx] = true;
                    if (!ignoreDeepWater && (liq->flags[y][x] & (1 << 7)))
                        out.liquid_flags[iy][ix] |= kLiquidFlagDarkWater;
                    ++count;
                }
            }
        }
        uint32_t cFlag = mcnk->flags;
        if (cFlag & (1 << 2)) { out.liquid_entry[iy][ix] = 1; out.liquid_flags[iy][ix] |= kLiquidFlagWater; }
        if (cFlag & (1 << 3)) { out.liquid_entry[iy][ix] = 2; out.liquid_flags[iy][ix] |= kLiquidFlagOcean; }
        if (cFlag & (1 << 4)) { out.liquid_entry[iy][ix] = 3; out.liquid_flags[iy][ix] |= kLiquidFlagMagma; }

        for (int y = 0; y <= kAdtCellSize; ++y) {
            int cy = iy * kAdtCellSize + y;
            for (int x = 0; x <= kAdtCellSize; ++x) {
                int cx = ix * kAdtCellSize + x;
                out.liquid_height[cy][cx] = liq->liquid[y][x].height;
            }
        }
    }

    // Holes
    out.holes[iy][ix] = static_cast<uint16_t>(mcnk->holes);
    if (mcnk->holes) out.hasHoles = true;
}

// ─── ProcessMh2o ─────────────────────────────────────────────────────────────

void AdtReader::ProcessMh2o(const uint8_t* mh2oChunk, uint32_t /*payloadSize*/,
                             const LiquidDbcTables& dbc,
                             bool ignoreDeepWater,
                             AdtGridData& out)
{
    // MH2O chunk layout: fcc(4) + size(4) + adt_LIQUID_CELL[16][16]
    const uint8_t* base = mh2oChunk + 8; // skip fcc+size
    const adt_LIQUID_CELL* grid = reinterpret_cast<const adt_LIQUID_CELL*>(base);

    for (int i = 0; i < kAdtCellsPerGrid; ++i) {
        for (int j = 0; j < kAdtCellsPerGrid; ++j) {
            const adt_LIQUID_CELL& cell = grid[i * kAdtCellsPerGrid + j];
            if (!cell.used || !cell.OffsetInstances) continue;

            const adt_liquid_instance* h = reinterpret_cast<const adt_liquid_instance*>(
                base + cell.OffsetInstances);

            // Exists bitmap (64 bits, 1 = visible)
            uint64_t existsMask = 0xFFFFFFFFFFFFFFFFull;
            if (h->OffsetExistsBitmap)
                std::memcpy(&existsMask, base + h->OffsetExistsBitmap, 8);

            int count = 0;
            uint64_t mask = existsMask;
            for (int y = 0; y < h->GetEffectiveHeight(); ++y) {
                int cy = i * kAdtCellSize + y + h->GetEffectiveOffsetY();
                for (int x = 0; x < h->GetEffectiveWidth(); ++x) {
                    int cx = j * kAdtCellSize + x + h->GetEffectiveOffsetX();
                    if (mask & 1u) { out.liquid_show[cy][cx] = true; ++count; }
                    mask >>= 1u;
                }
            }

            // Determine WoW liquid type (SoundBank)
            LiquidVertexFormat fmt = GetLiquidVertexFormat(base, h, dbc);
            uint16_t liqType = (fmt == LiquidVertexFormat::Depth) ? 2 : h->LiquidType;
            out.liquid_entry[i][j] = liqType;

            // Map LiquidType DBC SoundBank to map flags
            {
                auto it = dbc.types.find(liqType);
                if (it != dbc.types.end()) {
                    switch (it->second.SoundBank) {
                    case kLiquidSoundWater: out.liquid_flags[i][j] |= kLiquidFlagWater; break;
                    case kLiquidSoundOcean:
                        out.liquid_flags[i][j] |= kLiquidFlagOcean;
                        if (!ignoreDeepWater) {
                            adt_liquid_attributes attrs{};
                            if (cell.OffsetAttributes)
                                std::memcpy(&attrs, base + cell.OffsetAttributes, sizeof(attrs));
                            else
                                attrs = {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull};
                            if (attrs.Deep)
                                out.liquid_flags[i][j] |= kLiquidFlagDarkWater;
                        }
                        break;
                    case kLiquidSoundMagma: out.liquid_flags[i][j] |= kLiquidFlagMagma; break;
                    case kLiquidSoundSlime: out.liquid_flags[i][j] |= kLiquidFlagSlime; break;
                    default: break;
                    }
                }
            }

            // Height values
            int pos = 0;
            for (int y = 0; y <= h->GetEffectiveHeight(); ++y) {
                int cy = i * kAdtCellSize + y + h->GetEffectiveOffsetY();
                for (int x = 0; x <= h->GetEffectiveWidth(); ++x) {
                    int cx = j * kAdtCellSize + x + h->GetEffectiveOffsetX();
                    out.liquid_height[cy][cx] = GetMh2oLiquidHeight(base, h, fmt, pos);
                    ++pos;
                }
            }
        }
    }
}

// ─── Parse ───────────────────────────────────────────────────────────────────

bool AdtReader::Parse(const uint8_t* buf, uint32_t size,
                      const LiquidDbcTables& dbc,
                      bool ignoreDeepWater,
                      AdtGridData& out)
{
    if (!buf || size < 12) return false;

    // Validate MVER chunk (first chunk, 12 bytes total: fcc+size+ver)
    uint32_t fcc0 = Read32(buf);
    if (fcc0 != kTagMVER) return false;
    uint32_t ver = Read32(buf + 8);
    if (ver != 18) return false;

    std::memset(&out, 0, sizeof(out));

    // Iterate all top-level chunks
    uint32_t pos = 0;
    while (pos + 8 <= size) {
        uint32_t tag  = Read32(buf + pos);
        uint32_t csz  = Read32(buf + pos + 4);
        if (pos + 8 + csz > size) break;

        if (tag == kTagMCNK) {
            ProcessMcnk(buf + pos, ignoreDeepWater, out);
        } else if (tag == kTagMH2O) {
            ProcessMh2o(buf + pos, csz, dbc, ignoreDeepWater, out);
        } else if (tag == kTagMFBO) {
            // Flight box: 2 planes of 9 int16 each
            const uint8_t* mfbo = buf + pos + 8;
            std::memcpy(out.flight_box_max, mfbo, 18);
            std::memcpy(out.flight_box_min, mfbo + 18, 18);
            out.hasFlightBox = true;
        }
        pos += 8 + csz;
    }
    return true;
}

} // namespace Firelands::VMap::MapExtractor
