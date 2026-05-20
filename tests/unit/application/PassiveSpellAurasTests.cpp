#include <gtest/gtest.h>
#include <application/spell/PassiveSpellAuras.h>
#include <domain/models/SpellDefinition.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/StarterSpellFilters.h>

using namespace Firelands;

namespace {

class PassiveSpellStore final : public ISpellDefinitionStore {
public:
  explicit PassiveSpellStore(SpellDefinition def) : m_def(std::move(def)) {}

  bool HasSpell(uint32 spellId) const override { return spellId == m_def.id; }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    if (spellId == m_def.id)
      return m_def;
    return std::nullopt;
  }

  SpellDefinition m_def;
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

  PassiveSpellStore store(def);
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

  PassiveSpellStore store(def);
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

  PassiveSpellStore store(passive);
  auto outcomes =
      BuildPassiveAuraOutcomes(0x100ULL, 1, {78u, 668u, 20572u}, &store, nullptr,
                               std::chrono::steady_clock::now());
  ASSERT_EQ(outcomes.size(), 1u);
  EXPECT_EQ(outcomes[0].auraSpellId, 20572u);
}
