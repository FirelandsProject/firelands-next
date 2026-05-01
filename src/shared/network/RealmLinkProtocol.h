#ifndef FIRELANDS_SHARED_NETWORK_REALM_LINK_PROTOCOL_H
#define FIRELANDS_SHARED_NETWORK_REALM_LINK_PROTOCOL_H

#include <cstdint>

namespace Firelands {

/// Binary handshake world → auth (internal link, not WoW client protocol).
/// Layout: magic (4 LE) + tokenLen (2 LE) + token + realmId (4 LE).
inline constexpr uint32_t kRealmLinkMagic = 0x01774C46u; // 'F','L','W', 0x01

inline constexpr uint8_t kRealmLinkAckOk = 0x01;
inline constexpr uint8_t kRealmLinkAckReject = 0x00;

/// Optional keepalive payload after handshake (ignored by auth).
inline constexpr uint8_t kRealmLinkPing = 0x70;

inline constexpr uint16_t kRealmLinkMaxTokenLen = 256;

} // namespace Firelands

#endif
