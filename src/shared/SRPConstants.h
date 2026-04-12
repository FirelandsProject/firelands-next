#ifndef FIRELANDS_SHARED_SRP_CONSTANTS_H
#define FIRELANDS_SHARED_SRP_CONSTANTS_H

#include <shared/Common.h>
#include <vector>

namespace Firelands {
namespace SRP {

    // The Safe Prime (N) used by World of Warcraft (Cataclysm/WotLK/Retail SRP)
    // Value: 0x894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7
    const std::vector<uint8_t> N = {
        0x89, 0x4B, 0x64, 0x5E, 0x89, 0xE1, 0x53, 0x5B, 
        0xBD, 0xAD, 0x5B, 0x8B, 0x29, 0x06, 0x50, 0x53, 
        0x08, 0x01, 0xB1, 0x8E, 0xBF, 0xBF, 0x5E, 0x8F, 
        0xAB, 0x3C, 0x82, 0x87, 0x2A, 0x3E, 0x9B, 0xB7
    };

    // The Generator (g)
    constexpr uint8 g = 7;

    // k multiplier in SRP-6a: SHA1(N | g)
    // For WoW, it's often hardcoded to 3 or calculated as SHA1(N | PAD(g))
    constexpr uint8 k = 3;

} // namespace SRP
} // namespace Firelands

#endif // FIRELANDS_SHARED_SRP_CONSTANTS_H
