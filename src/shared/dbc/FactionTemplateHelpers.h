#pragma once

#include <shared/dbc/FactionTemplateDbc.h>
#include <cstdint>

namespace Firelands {

/// Bitmasks for `FactionTemplateEntry::factionGroup` / `friendGroup` / `enemyGroup`
/// (Trinity `FactionGroupMasks` / client `FactionTemplate.dbc`).
enum FactionGroupMask : uint32_t {
  FactionGroupMaskNone = 0x0,
  FactionGroupMaskPlayer = 0x1,
  FactionGroupMaskAlliance = 0x2,
  FactionGroupMaskHorde = 0x4,
  FactionGroupMaskMonster = 0x8,
};

enum FactionTemplateFlags : uint32_t {
  FactionTemplateFlagHatesAllExceptFriends = 0x2000,
  FactionTemplateFlagAttackPvPActivePlayers = 0x1000,
};

/// Trinity `FactionTemplateEntry::IsHostileToPlayers()` (mask on `enemyGroup`).
inline bool FactionTemplateHostileToPlayers(FactionTemplateEntry const &e) {
  return (e.enemyGroup & FactionGroupMaskPlayer) != 0;
}

/// Friendly to the generic "player" group mask (`friendGroup`).
inline bool FactionTemplateFriendlyToPlayers(FactionTemplateEntry const &e) {
  return (e.friendGroup & FactionGroupMaskPlayer) != 0;
}

/// Trinity-style: `HatesAllExceptFriends` implies hostility unless explicitly friendly.
inline bool FactionTemplateHatesAllExceptFriends(FactionTemplateEntry const &e) {
  return (e.flags & FactionTemplateFlagHatesAllExceptFriends) != 0;
}

/// Heuristic for "default PvE reaction toward a random player" (no reputation math).
inline bool FactionTemplateLikelyHostileToPlayers(FactionTemplateEntry const &e) {
  if (FactionTemplateHatesAllExceptFriends(e) && !FactionTemplateFriendlyToPlayers(e))
    return true;
  return FactionTemplateHostileToPlayers(e);
}

inline bool FactionTemplateLikelyFriendlyToPlayers(FactionTemplateEntry const &e) {
  return FactionTemplateFriendlyToPlayers(e) && !FactionTemplateHostileToPlayers(e);
}

/// Neither mask-based friendly nor heuristic hostility (still not full client reaction).
inline bool FactionTemplateLikelyNeutralToPlayers(FactionTemplateEntry const &e) {
  return !FactionTemplateLikelyHostileToPlayers(e) &&
         !FactionTemplateLikelyFriendlyToPlayers(e);
}

} // namespace Firelands
