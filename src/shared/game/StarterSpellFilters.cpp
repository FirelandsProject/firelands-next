#include <shared/game/StarterSpellFilters.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/game/SpellAuraTypes.h>

namespace Firelands {

bool IsMetaOrInternalStarterSkill(uint32_t skillId) {
  switch (skillId) {
  case 95u:   // Defense — playercreateinfo row; clutters profession UI when unlearned
  case 183u:  // GENERIC (DND)
  case 777u:  // Mounts
  case 778u:  // Companion Pets
  case 810u:  // All - Glyphs
    return true;
  default:
    return false;
  }
}

bool IsRidingStarterSkill(uint32_t skillId) {
  switch (skillId) {
  case 762u: // Riding
    return true;
  default:
    return false;
  }
}

bool IsProfessionStarterSkill(uint32_t skillId) {
  switch (skillId) {
  case 129u:  // First Aid
  case 164u:  // Blacksmithing
  case 165u:  // Leatherworking
  case 171u:  // Alchemy
  case 182u:  // Herbalism
  case 185u:  // Cooking
  case 186u:  // Mining
  case 197u:  // Tailoring
  case 202u:  // Engineering
  case 333u:  // Enchanting
  case 356u:  // Fishing
  case 393u:  // Skinning
  case 773u:  // Inscription
    return true;
  default:
    return false;
  }
}

bool IsClassSpellTabStarterSkill(uint32_t skillId) {
  switch (skillId) {
  case 6u:
  case 8u:
  case 38u:
  case 39u:
  case 50u:
  case 51u:
  case 56u:
  case 78u:
  case 134u:
  case 163u:
  case 184u:
  case 237u:
  case 253u:
  case 267u:
  case 354u:
  case 355u:
  case 373u:
  case 374u:
  case 375u:
  case 573u:
  case 574u:
  case 593u:
  case 594u:
  case 613u:
  case 770u:
  case 771u:
  case 772u:
  case 795u:
  case 796u:
  case 797u:
  case 798u:
  case 799u:
  case 800u:
  case 801u:
  case 802u:
  case 803u:
  case 804u:
    return true;
  default:
    return false;
  }
}

bool IsExcludedStarterSkill(uint32_t skillId) {
  if (IsMetaOrInternalStarterSkill(skillId) || IsSecondaryProfessionSkillLine(skillId) ||
      IsRidingStarterSkill(skillId) || IsProfessionStarterSkill(skillId) ||
      IsClassSpellTabStarterSkill(skillId))
    return true;
  if (SkillLineCategoriesLoaded())
    return !IsAllowedStarterSkillLine(skillId);
  return false;
}

bool IsWarlockQuestGatedSummonSpell(uint32_t spellId) {
  switch (spellId) {
  case 688u:  // Summon Imp — starter quest chain (ref Piercing the Veil / Tainted *)
  case 697u:  // Summon Felhunter
  case 712u:  // Summon Succubus
  case 691u:  // Summon Felsteed
  case 693u:  // Summon Doomguard
  case 698u:  // Ritual of Doom
    return true;
  default:
    return false;
  }
}

std::vector<uint32_t> WarlockQuestGatedSummonSpellIds() {
  return {688u, 697u, 712u, 691u, 693u, 698u};
}

bool IsGuildPerkSpell(uint32_t spellId) {
  if (spellId >= 78631u && spellId <= 78635u)
    return true;
  if (spellId >= 83940u && spellId <= 83968u)
    return true;
  if (spellId == 84038u)
    return true;
  return false;
}

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

bool IsAlwaysOnLoginAuraType(uint32_t auraEffectType) {
  if (IsExcludedLoginAuraType(auraEffectType))
    return false;
  switch (auraEffectType) {
  case kSpellAuraModStat:
  case kSpellAuraModPercentStat:
  case kSpellAuraModAttackPower:
  case kSpellAuraModAttackPowerPct:
  case kSpellAuraModRating:
  case kSpellAuraModResistance:
  case kSpellAuraModDamageDone:
  case kSpellAuraModDamagePercentDone:
  case kSpellAuraModDodgePercent:
  case kSpellAuraModIncreaseHealthPercent:
  case kSpellAuraModHealthRegenPercent:
  case kSpellAuraModRegenDuringCombat:
  case kSpellAuraModPowerRegenPercent:
  case kSpellAuraModCombatSpeedPct:
  case kSpellAuraMechanicDurationMod:
    return true;
  default:
    return false;
  }
}

} // namespace Firelands
