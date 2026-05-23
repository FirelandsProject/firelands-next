#pragma once

#include <shared/Common.h>
#include <shared/game/PlayerPowerType.h>

#include <algorithm>

namespace Firelands {

/// True when the spell's `Spell.dbc` `PowerType` matches the unit's primary POWER1 type.
inline bool SpellUsesCasterPrimaryPower(uint32 spellPowerType, uint8 casterPrimaryPowerType) {
  return spellPowerType == static_cast<uint32>(casterPrimaryPowerType);
}

/// Default primary power for a class when the session has no explicit snapshot byte.
inline uint8 DefaultCasterPrimaryPowerType(uint8 klass) {
  return static_cast<uint8>(GetDefaultPlayerPowerType(klass));
}

/// Cataclysm `Spell.dbc`: field 42 is the `SpellPower.dbc` row id; field 14 is the power
/// type enum (0–6) or, when field 42 is zero, sometimes another `SpellPower` row id.
inline void AssignSpellPowerFieldsFromDbc(uint32 field14, uint32 field42, uint32 &outPowerType,
                                                                                    uint32 &outSpellPowerId) {
    outSpellPowerId = field42 != 0u ? field42 : (field14 > 6u ? field14 : 0u);
    outPowerType = field14 <= 6u ? field14 : 0u;
}

/// Field 14 may store a `SpellPower` row id; use the caster's primary power for cost math.
inline uint32 EffectiveSpellPowerTypeForCast(uint32 spellDbcPowerType,
                                                                                          uint8 casterPrimaryPowerType) {
    return spellDbcPowerType <= 6u ? spellDbcPowerType
                                                                  : static_cast<uint32>(casterPrimaryPowerType);
}

/// `% of max POWER1` needs a non-trivial pool; login snapshots can be 0/1 before templates apply.
inline uint32 MaxPower1ForSpellPercentCost(uint8 casterPrimaryPowerType, uint8 casterLevel,
                                                                                      uint32 liveMaxPower1) {
    if (liveMaxPower1 > 1u)
        return liveMaxPower1;
    if (casterPrimaryPowerType != static_cast<uint8>(PlayerPowerType::Mana))
        return std::max<uint32>(1u, liveMaxPower1);
    uint8 const lvl = std::max<uint8>(1, casterLevel);
    return std::max<uint32>(100u, 50u + static_cast<uint32>(lvl) * 15u);
}

} // namespace Firelands
