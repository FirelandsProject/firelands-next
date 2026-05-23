#ifndef FIRELANDS_SHARED_NETWORK_ACCOUNT_DATA_TYPES_H
#define FIRELANDS_SHARED_NETWORK_ACCOUNT_DATA_TYPES_H

#include <cstdint>
#include <string>

namespace Firelands {

struct AccountDataSlot {
  uint32_t time = 0;
  std::string data;
};

/// Cataclysm 4.3.4 account data blocks (same layout as firelands-cata-ref).
enum AccountDataType : uint8 {
  ACCOUNT_DATA_GLOBAL_CONFIG = 0,
  ACCOUNT_DATA_PER_CHARACTER_CONFIG = 1,
  ACCOUNT_DATA_GLOBAL_BINDINGS = 2,
  ACCOUNT_DATA_PER_CHARACTER_BINDINGS = 3,
  ACCOUNT_DATA_GLOBAL_MACROS = 4,
  ACCOUNT_DATA_PER_CHARACTER_MACROS = 5,
  ACCOUNT_DATA_PER_CHARACTER_LAYOUT = 6,
  ACCOUNT_DATA_PER_CHARACTER_CHAT = 7,
  NUM_ACCOUNT_DATA_TYPES = 8,
};

/// Stored in `firelands_auth.account_data` (per account).
constexpr uint32_t kGlobalAccountDataMask = 0x15u; // bits 0, 2, 4

/// Stored in `firelands_characters.character_account_data` (per character guid).
constexpr uint32_t kPerCharacterAccountDataMask = 0xEAu; // bits 1, 3, 5, 6, 7

inline bool IsGlobalAccountDataType(uint32_t type) {
  return type < NUM_ACCOUNT_DATA_TYPES &&
         ((1u << type) & kGlobalAccountDataMask) != 0;
}

inline bool IsPerCharacterAccountDataType(uint32_t type) {
  return type < NUM_ACCOUNT_DATA_TYPES &&
         ((1u << type) & kPerCharacterAccountDataMask) != 0;
}

} // namespace Firelands

#endif
