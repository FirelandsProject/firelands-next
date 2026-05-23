#include <gtest/gtest.h>
#include <application/spell/PlayerAuraRegenEffects.h>
#include <domain/models/SpellDefinition.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Aura.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/SpellAuraTypes.h>

#include <chrono>
#include <unordered_map>

using namespace Firelands;

namespace {

class RegenAuraStore final : public ISpellDefinitionStore {
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

Aura MakeAura(uint32 spellId) {
  auto const farFuture = std::chrono::steady_clock::time_point::max();
  return Aura(spellId, kSpellAuraModHealthRegenPercent, 0, 0, 1, farFuture, 0, 0, 0,
              farFuture, {});
}

} // namespace

TEST(PlayerAuraRegenEffectsTests, TrollRegenerationSpellAggregatesBothRows) {
  SpellDefinition def{};
  def.id = 20555u;
  def.attributes = SpellAttr0::kPassive;
  SpellAuraEffectRow pct{};
  pct.auraType = kSpellAuraModHealthRegenPercent;
  pct.basePoints = 10;
  SpellAuraEffectRow combat{};
  combat.auraType = kSpellAuraModRegenDuringCombat;
  combat.basePoints = 10;
  def.auraEffects.push_back(pct);
  def.auraEffects.push_back(combat);

  RegenAuraStore store;
  store.Add(def);

  ResourceRegenModifiers const mods =
      ComputePlayerResourceRegenModifiers({MakeAura(20555u)}, &store, 85);
  EXPECT_EQ(mods.healthRegenPct, 11);
  EXPECT_EQ(mods.healthRegenDuringCombatPct, 11);
}
