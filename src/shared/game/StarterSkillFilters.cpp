#include <shared/game/StarterSkillFilters.h>
#include <shared/game/SkillLineCategories.h>

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

} // namespace Firelands
