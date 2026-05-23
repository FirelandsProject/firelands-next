#include <gtest/gtest.h>
#include <shared/game/CombatDamage.h>
#include <shared/game/SpellSchool.h>
#include <shared/game/UnitCombatStats.h>

using namespace Firelands;

TEST(CombatDamageTests, ArmorCapsAtSeventyFivePercent) {
  uint32 const raw = 1000u;
  uint32 const mitigated = CalcArmorReducedDamage(85, 50000u, raw);
  EXPECT_LE(mitigated, 265u);
  EXPECT_GT(mitigated, 0u);
}

TEST(CombatDamageTests, LowLevelTargetTakesLessMitigationThanHighArmor) {
  uint32 const vsCloth = CalcArmorReducedDamage(10, 200u, 100u);
  uint32 const vsPlate = CalcArmorReducedDamage(10, 3000u, 100u);
  EXPECT_GT(vsCloth, vsPlate);
}

TEST(CombatDamageTests, ResistanceBuffModsReduceMagicDamage) {
  UnitCombatStats caster{};
  caster.level = 10;

  UnitCombatStats victim{};
  victim.level = 10;
  victim.resistance[5] = 0u;
  victim.resistanceBuffPos[5] = 100;

  int32 const mitigated = ResolveMitigatedHealthDelta(
      -100, kSpellSchoolMaskShadow, &caster, caster.level, &victim);
  EXPECT_LT(-mitigated, 100);
}

TEST(CombatDamageTests, MagicResistReducesFireDamage) {
  UnitCombatStats caster{};
  caster.level = 10;

  UnitCombatStats victim{};
  victim.level = 10;
  victim.resistance[2] = 100u;

  int32 const mitigated = ResolveMitigatedHealthDelta(
      -100, kSpellSchoolMaskFire, &caster, caster.level, &victim);
  EXPECT_LT(-mitigated, 100);
}

TEST(CombatDamageTests, SpellPowerIncreasesOutgoingMagicDamage) {
  UnitCombatStats weakCaster{};
  weakCaster.level = 20;

  UnitCombatStats strongCaster = weakCaster;
  strongCaster.spellDamageDonePos[2] = 200;

  UnitCombatStats victim{};
  victim.level = 20;
  victim.resistance[2] = 0;

  int32 const weak = ResolveMitigatedHealthDelta(-50, kSpellSchoolMaskFire, &weakCaster,
                                                 weakCaster.level, &victim);
  int32 const strong =
      ResolveMitigatedHealthDelta(-50, kSpellSchoolMaskFire, &strongCaster,
                                  strongCaster.level, &victim);
  EXPECT_GT(weak, strong);
}

TEST(CombatDamageTests, HealsAreNotMitigated) {
  EXPECT_EQ(ResolveMitigatedHealthDelta(25, kSpellSchoolMaskHoly, nullptr, 1, nullptr),
            25);
}

TEST(CombatDamageTests, MeleeSwingUsesAttackPowerAndArmor) {
  UnitCombatStats attacker{};
  attacker.level = 20;
  attacker.attackPower = 200;

  UnitCombatStats softTarget{};
  softTarget.level = 20;
  softTarget.armor = 100u;

  UnitCombatStats armoredTarget = softTarget;
  armoredTarget.armor = 4000u;

  uint32 const vsSoft = ComputeMeleeSwingDamage(attacker, softTarget);
  uint32 const vsArmored = ComputeMeleeSwingDamage(attacker, armoredTarget);
  EXPECT_GT(vsSoft, vsArmored);
  EXPECT_GT(vsSoft, 10u);
}
