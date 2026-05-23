#pragma once

#include <domain/models/PlayerCreateInfo.h>

#include <cstdint>
#include <vector>

namespace Firelands {

/// WoW `ItemClass` values for `SMSG_SET_PROFICIENCY` (4.3.4).
constexpr uint8_t kItemClassWeapon = 2u;
constexpr uint8_t kItemClassArmor = 4u;

/// Builds weapon/armor subclass masks from starter skill grants (`Player` logic).
uint32_t ComputeWeaponProficiencyMask(
    std::vector<StarterSkillGrant> const &skills);
uint32_t ComputeArmorProficiencyMask(
    std::vector<StarterSkillGrant> const &skills);

} // namespace Firelands
