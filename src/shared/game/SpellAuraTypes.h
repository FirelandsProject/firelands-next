#pragma once

#include <shared/Common.h>

namespace Firelands {

/// `SpellEffect.dbc` effect types (subset).
constexpr uint32 kSpellEffectApplyAura = 6u;

/// `SpellEffect.dbc` aura names (`EffectAura` / misc) — Cataclysm 4.3.4 (Trinity
/// `SpellAuraDefines.h`).
constexpr uint32 kSpellAuraPeriodicDamage = 3u;
constexpr uint32 kSpellAuraPeriodicHeal = 8u;
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

/// `SpellEffect.dbc` effect types — grants a skill line (profession trainers).
constexpr uint32 kSpellEffectSkill = 118u;

} // namespace Firelands
