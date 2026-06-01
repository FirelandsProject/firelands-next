#include <gtest/gtest.h>
#include <domain/models/SpellDefinition.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/SpellAuraTypes.h>

using namespace Firelands;

TEST(SpellDefinitionTests, PassiveAndCantCancelFromAttributes) {
  SpellDefinition passive{};
  passive.attributes = SpellAttr0::kPassive;
  EXPECT_TRUE(passive.isPassiveSpell());
  EXPECT_TRUE(passive.playerCanCancelAuraByClient());

  SpellDefinition noCancel{};
  noCancel.attributes = SpellAttr0::kCantCancel;
  EXPECT_FALSE(noCancel.playerCanCancelAuraByClient());
  EXPECT_FALSE(noCancel.isPassiveSpell());

  SpellDefinition both{};
  both.attributes = SpellAttr0::kPassive | SpellAttr0::kCantCancel;
  EXPECT_TRUE(both.isPassiveSpell());
  EXPECT_FALSE(both.playerCanCancelAuraByClient());
}

TEST(SpellDefinitionTests, ActivatablePassiveUsesSpellCooldownRow) {
  SpellDefinition bloodFury{};
  bloodFury.attributes = SpellAttr0::kPassive;
  bloodFury.cooldownsId = 1u;
  EXPECT_TRUE(bloodFury.isActivatablePassiveSpell());
  EXPECT_FALSE(bloodFury.isPermanentLoginPassiveSpell());

  SpellDefinition hardiness{};
  hardiness.attributes = SpellAttr0::kPassive;
  hardiness.durationIndex = 21u;
  EXPECT_FALSE(hardiness.isActivatablePassiveSpell());
  EXPECT_TRUE(hardiness.isPermanentLoginPassiveSpell());
}

TEST(SpellDefinitionTests, DaVoodooShuffleQualifiesAsAlwaysOnLoginPassive) {
  SpellDefinition daVoodoo{};
  daVoodoo.attributes = 0x140u;
  daVoodoo.durationIndex = 0u;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraMechanicDurationMod;
  row.basePoints = -15;
  daVoodoo.auraEffects.push_back(row);

  EXPECT_FALSE(daVoodoo.isPassiveSpell());
  EXPECT_FALSE(daVoodoo.isPermanentLoginPassiveSpell());
  EXPECT_TRUE(daVoodoo.isAlwaysOnLoginPassiveSpell());
}
