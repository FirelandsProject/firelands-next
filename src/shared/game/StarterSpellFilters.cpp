#include <shared/game/StarterSpellFilters.h>
#include <shared/game/SpellAuraTypes.h>

namespace Firelands {

bool IsRidingOrTransportStarterSpell(uint32_t spellId) {
  switch (spellId) {
  case 33388u:
  case 33391u:
  case 34090u:
  case 34091u:
  case 54197u:
  case 90265u:
  case 90267u:
  case 40120u:
  case 33943u:
  case 86470u:
  case 86530u:
    return true;
  default:
    return false;
  }
}

bool IsKnownMountSpell(uint32_t spellId) {
  switch (spellId) {
  case 55531u:  // Mechano-Hog
  case 60424u:  // Mekgineer's Chopper
  case 93644u:  // Kor'kron Annihilator
  case 61425u:  // Swift Shorestrider
  case 74918u:  // Wooly White Rhino
  case 87090u:  // Goblin Trike
  case 87091u:
  case 67336u:  // Sunreaver Hawkstrider
    return true;
  default:
    return false;
  }
}

bool IsClassShapeshiftStarterSpell(uint32_t spellId) {
  switch (spellId) {
  case 768u:   // Cat Form
  case 5487u:  // Bear Form
  case 783u:   // Travel Form
  case 1066u:  // Aquatic Form
  case 9634u:  // Dire Bear Form
  case 24858u: // Moonkin Form
  case 33891u: // Tree of Life
    return true;
  default:
    return false;
  }
}

bool IsMountOrVehicleAuraType(uint32_t auraEffectType) {
  switch (auraEffectType) {
  case kSpellAuraModIncreaseMountedSpeed:
  case kSpellAuraMounted:
  case kSpellAuraModMountedSpeedAlways:
  case kSpellAuraModMountedSpeedNotStack:
  case kSpellAuraModIncreaseMountedFlightSpeed:
  case kSpellAuraModMountedFlightSpeedAlways:
  case kSpellAuraControlVehicle:
  case kSpellAuraSetVehicleId:
  case kSpellAuraCosmeticMounted:
  case kSpellAuraFly:
    return true;
  default:
    return false;
  }
}

bool IsExcludedLoginAuraType(uint32_t auraEffectType) {
  if (IsMountOrVehicleAuraType(auraEffectType))
    return true;
  switch (auraEffectType) {
  case kSpellAuraModShapeshift:
  case kSpellAuraTransform:
    return true;
  default:
    return false;
  }
}

} // namespace Firelands
