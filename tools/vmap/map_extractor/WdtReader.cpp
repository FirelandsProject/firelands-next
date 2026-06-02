#include "WdtReader.h"

#include <cstring>

namespace Firelands::VMap::MapExtractor {

static constexpr uint32_t RawTag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(d) << 24u) |
           (static_cast<uint32_t>(c) << 16u) |
           (static_cast<uint32_t>(b) <<  8u) |
           (static_cast<uint32_t>(a));
}
static constexpr uint32_t kTagMVER = RawTag('R','E','V','M');
static constexpr uint32_t kTagMAIN = RawTag('N','I','A','M');

static inline uint32_t Read32(const uint8_t* p) {
    uint32_t v; std::memcpy(&v, p, 4); return v;
}

bool WdtReader::Parse(const uint8_t* buf, uint32_t size, WdtTileGrid& out) {
    if (!buf || size < 12) return false;

    // First chunk must be MVER with version == 18
    if (Read32(buf) != kTagMVER) return false;
    if (Read32(buf + 8) != 18)   return false;

    out.exists.reset();

    uint32_t pos = 0;
    while (pos + 8 <= size) {
        uint32_t tag = Read32(buf + pos);
        uint32_t csz = Read32(buf + pos + 4);
        if (pos + 8 + csz > size) break;

        if (tag == kTagMAIN) {
            // MAIN payload is an array of wdt_MAIN::adtData structs (8 bytes each)
            // Layout: flag[4] + data1[4] per cell, row-major [64][64]
            const uint8_t* payload = buf + pos + 8;
            for (int y = 0; y < kWdtMapSize; ++y) {
                for (int x = 0; x < kWdtMapSize; ++x) {
                    uint32_t flag = Read32(payload + (y * kWdtMapSize + x) * 8);
                    if (flag & 0x1u)
                        out.exists.set(static_cast<size_t>(y) * kWdtMapSize + x);
                }
            }
        }
        pos += 8 + csz;
    }
    return true;
}

} // namespace Firelands::VMap::MapExtractor
