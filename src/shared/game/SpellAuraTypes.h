#pragma once

#include <shared/Common.h>

namespace Firelands {

/// `SpellEffect.dbc` effect types (subset).
constexpr uint32 kSpellEffectApplyAura = 6u;

/// `SpellEffect.dbc` aura names (`EffectAura` / misc) — Cataclysm 4.3.4.
/// `SpellAuraDefines.h`).
constexpr uint32 kSpellAuraPeriodicDamage = 3u;
constexpr uint32 kSpellAuraPeriodicHeal = 8u;
constexpr uint32 kSpellAuraModStat = 29u;
constexpr uint32 kSpellAuraModPercentStat = 87u;
constexpr uint32 kSpellAuraModAttackPowerPct = 99u;
constexpr uint32 kSpellAuraModAttackPower = 124u;
constexpr uint32 kSpellAuraModRating = 189u;
constexpr uint32 kSpellAuraModResistance = 22u;
constexpr uint32 kSpellAuraModDamageDone = 13u;
constexpr uint32 kSpellAuraModDodgePercent = 49u;
constexpr uint32 kSpellAuraModDamagePercentDone = 79u;
constexpr uint32 kSpellAuraModIncreaseHealthPercent = 133u;
constexpr uint32 kSpellAuraModPowerRegen = 85u;
constexpr uint32 kSpellAuraModHealthRegenPercent = 88u;
constexpr uint32 kSpellAuraModPowerRegenPercent = 110u;
constexpr uint32 kSpellAuraModRegenDuringCombat = 116u;
constexpr uint32 kSpellAuraModShapeshift = 36u;
constexpr uint32 kSpellAuraTransform = 56u;
constexpr uint32 kSpellAuraModIncreaseMountedSpeed = 32u;
constexpr uint32 kSpellAuraMounted = 78u;
constexpr uint32 kSpellAuraFly = 201u;
constexpr uint32 kSpellAuraModMountedSpeedAlways = 130u;
constexpr uint32 kSpellAuraModMountedSpeedNotStack = 172u;
constexpr uint32 kSpellAuraModIncreaseMountedFlightSpeed = 207u;
constexpr uint32 kSpellAuraModMountedFlightSpeedAlways = 209u;
constexpr uint32 kSpellAuraControlVehicle = 236u;
constexpr uint32 kSpellAuraSetVehicleId = 296u;
constexpr uint32 kSpellAuraCosmeticMounted = 487u;
/// `SpellAuraDefines.h` — Cataclysm phase visibility.
constexpr uint32 kSpellAuraPhase = 261u;
constexpr uint32 kSpellAuraPhaseGroup = 326u;
constexpr uint32 kSpellAuraPhaseAlwaysVisible = 327u;

/// `SpellEffect.dbc` effect types — grants a skill line (profession trainers).
constexpr uint32 kSpellEffectSkill = 118u;
constexpr uint32 kSpellEffectSchoolDamage = 2u;
constexpr uint32 kSpellEffectHealthLeech = 9u;
constexpr uint32 kSpellEffectHeal = 10u;
constexpr uint32 kSpellEffectEnvironmentalDamage = 13u;
constexpr uint32 kSpellEffectEnergize = 30u;

inline bool SpellEffectKindHasImmediateHealthDelta(uint32 effectKind) {
  return effectKind == kSpellEffectSchoolDamage ||
         effectKind == kSpellEffectHeal || effectKind == kSpellEffectHealthLeech ||
         effectKind == kSpellEffectEnvironmentalDamage;
}

} // namespace Firelands
