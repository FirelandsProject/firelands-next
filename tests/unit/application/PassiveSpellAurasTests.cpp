#include <gtest/gtest.h>
#include <application/spell/PassiveSpellAuras.h>
#include <domain/models/SpellDefinition.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/game/StarterSpellFilters.h>
#include <unordered_set>

using namespace Firelands;

namespace {

class PassiveSpellStore final : public ISpellDefinitionStore {
public:
  void Add(SpellDefinition def) { m_defs[def.id] = std::move(def); }

  bool HasSpell(uint32 spellId) const override {
    return m_defs.find(spellId) != m_defs.end();
  }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    auto it = m_defs.find(spellId);
    if (it == m_defs.end())
      return std::nullopt;
    return it->second;
  }

  std::unordered_map<uint32, SpellDefinition> m_defs;
};

} // namespace

TEST(PassiveSpellAurasTests, BuildsOutcomeForPassiveWithAura) {
  SpellDefinition def{};
  def.id = 20572u;
  def.attributes = SpellAttr0::kPassive;
  def.hasAuraEffect = true;
  def.auraEffectType = 99u;
  def.auraBasePoints = 5;
  def.auraDieSides = 0;

  PassiveSpellStore store;
  store.Add(def);
  auto outcomes = BuildPassiveAuraOutcomes(0x100ULL, 10, {20572u}, &store, nullptr,
                                           std::chrono::steady_clock::now());
  ASSERT_EQ(outcomes.size(), 1u);
  EXPECT_TRUE(outcomes[0].hasAuraApply);
  EXPECT_EQ(outcomes[0].auraSpellId, 20572u);
  EXPECT_EQ(outcomes[0].auraTargetGuid, 0x100ULL);
}

TEST(PassiveSpellAurasTests, SkipsMountAuraType) {
  SpellDefinition def{};
  def.id = 99999u;
  def.attributes = SpellAttr0::kPassive;
  def.hasAuraEffect = true;
  def.auraEffectType = 32u;

  PassiveSpellStore store;
  store.Add(def);
  auto outcomes = BuildPassiveAuraOutcomes(0x100ULL, 1, {99999u}, &store, nullptr,
                                           std::chrono::steady_clock::now());
  EXPECT_TRUE(outcomes.empty());
}

TEST(PassiveSpellAurasTests, SkipsNonPassiveAndLanguageSpells) {
  SpellDefinition active{};
  active.id = 78u;
  active.hasAuraEffect = true;

  SpellDefinition passive{};
  passive.id = 20572u;
  passive.attributes = SpellAttr0::kPassive;
  passive.hasAuraEffect = true;

  PassiveSpellStore store;
  store.Add(active);
  store.Add(passive);
  auto outcomes =
      BuildPassiveAuraOutcomes(0x100ULL, 1, {78u, 668u, 20572u}, &store, nullptr,
                               std::chrono::steady_clock::now());
  ASSERT_EQ(outcomes.size(), 1u);
  EXPECT_EQ(outcomes[0].auraSpellId, 20572u);
}

TEST(PassiveSpellAurasTests, CollectLoginPassiveSpellIds_FiltersActiveAndLanguage) {
  SpellDefinition active{};
  active.id = 78u;
  active.hasAuraEffect = true;

  SpellDefinition permanentPassive{};
  permanentPassive.id = 20573u;
  permanentPassive.attributes = SpellAttr0::kPassive;
  permanentPassive.hasAuraEffect = true;
  permanentPassive.auraEffectType = 99u;

  SpellDefinition activatablePassive{};
  activatablePassive.id = 20572u;
  activatablePassive.attributes = SpellAttr0::kPassive;
  activatablePassive.cooldownsId = 1u;
  activatablePassive.hasAuraEffect = true;
  activatablePassive.auraEffectType = 99u;

  SpellDefinition mountPassive{};
  mountPassive.id = 99999u;
  mountPassive.attributes = SpellAttr0::kPassive;
  mountPassive.hasAuraEffect = true;
  mountPassive.auraEffectType = kSpellAuraModIncreaseMountedSpeed;

  PassiveSpellStore store;
  store.Add(active);
  store.Add(permanentPassive);
  store.Add(activatablePassive);
  store.Add(mountPassive);

  std::unordered_set<uint32> known{78u, 668u, 20572u, 20573u, 99999u};
  auto ids = CollectLoginPassiveSpellIds(known, &store);
  ASSERT_EQ(ids.size(), 1u);
  EXPECT_EQ(ids[0], 20573u);
}
