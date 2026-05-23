#include <gtest/gtest.h>
#include <domain/models/SpellDefinition.h>
#include <shared/game/SpellAttributes.h>

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
