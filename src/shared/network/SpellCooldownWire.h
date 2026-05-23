#pragma once

#include <shared/Common.h>
#include <shared/network/WorldPacket.h>
#include <vector>

namespace Firelands {
namespace SpellCooldownWire {

/// `SMSG_SPELL_COOLDOWN` flags (Cataclysm 4.3.4).
enum SpellCooldownFlags : uint8 {
  kNone = 0,
  kIncludeGcd = 1,
};

/// Cataclysm 4.3.4: `SpellCooldownStruct` (per spell row in `SMSG_SPELL_COOLDOWN`).
struct SpellCooldownStruct {
  uint32 spellId = 0;
  /// Remaining cooldown in milliseconds (`ForcedCooldown` on the client).
  uint32 forcedCooldownMs = 0;
  float modRate = 1.0f;
};

/// Legacy alias used by session code.
struct SpellCooldownEntry {
  uint32 spellId = 0;
  int32 remainingMs = 0;
};

/// Builds `SMSG_COOLDOWN_EVENT` — starts the client cooldown UI (action bar + spellbook).
void BuildCooldownEvent(WorldPacket &out, int32 spellId, bool isPet = false);

/// Builds `SMSG_SPELL_COOLDOWN` (packed caster guid, flags, count, struct rows).
void BuildSpellCooldown(WorldPacket &out, uint64 unitGuid, uint8 flags,
                        std::vector<SpellCooldownStruct> const &entries);

/// Convenience: single-row `SMSG_SPELL_COOLDOWN`.
void BuildSpellCooldown(WorldPacket &out, uint64 unitGuid, uint8 flags,
                        SpellCooldownStruct const &entry);

/// Builds `SMSG_CLEAR_COOLDOWNS` (4.3.4 scrambled guid + spell id list).
void BuildClearCooldowns(WorldPacket &out, uint64 unitGuid, uint32 const *spellIds,
                         size_t spellIdCount);

struct CategoryCooldownEntry {
  int32 category = 0;
  int32 remainingMs = 0;
};

/// Builds `SMSG_CATEGORY_COOLDOWN` (count in 23 bits, then category + remaining pairs).
void BuildCategoryCooldown(WorldPacket &out, CategoryCooldownEntry const *entries,
                           size_t entryCount);

} // namespace SpellCooldownWire
} // namespace Firelands
