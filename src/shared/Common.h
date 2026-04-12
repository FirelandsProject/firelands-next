#ifndef FIRELANDS_SHARED_COMMON_H
#define FIRELANDS_SHARED_COMMON_H

#include <cstdint>

namespace Firelands {

    // Common WoW Types
    using uint64 = std::uint64_t;
    using uint32 = std::uint32_t;
    using uint16 = std::uint16_t;
    using uint8  = std::uint8_t;
    
    using int64 = std::int64_t;
    using int32 = std::int32_t;
    using int16 = std::int16_t;
    using int8  = std::int8_t;

    // WoW Version Constants (4.3.4.15595)
    constexpr uint32 CLIENT_BUILD = 15595;
    constexpr uint8  CLIENT_VERSION_MAJOR = 4;
    constexpr uint8  CLIENT_VERSION_MINOR = 3;
    constexpr uint8  CLIENT_VERSION_PATCH = 4;

} // namespace Firelands

#endif // FIRELANDS_SHARED_COMMON_H
