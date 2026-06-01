#include <gtest/gtest.h>
#include <application/spell/PlayerAuraStatEffects.h>
#include <domain/models/SpellDefinition.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <domain/world/Aura.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/game/SpellAttributes.h>
#include <shared/game/UnitCombatStats.h>
#include <array>
#include <unordered_map>
#include <unordered_set>

using namespace Firelands;

namespace {

class StatAuraStore final : public ISpellDefinitionStore {
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

Aura MakePassiveAura(uint32 spellId) {
  auto const farFuture = std::chrono::steady_clock::time_point::max();
  return Aura(spellId, kSpellAuraModStat, 3, 0, 0x100ULL, farFuture, 0, 0, 0,
              farFuture, {});
}

} // namespace

TEST(PlayerAuraStatEffectsTests, ModStatAddsPosStatFromAuraEffectsRow) {
  SpellDefinition def{};
  def.id = 100u;
  def.attributes = SpellAttr0::kPassive;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModStat;
  row.basePoints = 2;
  row.miscValue = 1;
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(100u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 10);

  EXPECT_EQ(bonus.posStat[1], 3);
  EXPECT_EQ(bonus.posStat[0], 0);
}

TEST(PlayerAuraStatEffectsTests, ModAttackPowerAddsPosMod) {
  SpellDefinition def{};
  def.id = 20572u;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModAttackPower;
  row.basePoints = 6;
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(20572u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 10);

  EXPECT_EQ(bonus.attackPowerModPos, 7);
  EXPECT_EQ(bonus.attackPowerModNeg, 0);
  EXPECT_FLOAT_EQ(bonus.attackPowerMultiplier, 0.f);
}

TEST(PlayerAuraStatEffectsTests, ModAttackPowerPctAddsMultiplier) {
  SpellDefinition def{};
  def.id = 20572u;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModAttackPowerPct;
  row.basePoints = 6;
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(20572u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 10);

  EXPECT_EQ(bonus.attackPowerModPos, 0);
  EXPECT_FLOAT_EQ(bonus.attackPowerMultiplier, 0.07f);
}

TEST(PlayerAuraStatEffectsTests, BloodFuryStacksFlatAndPctRows) {
  SpellDefinition def{};
  def.id = 20572u;
  SpellAuraEffectRow flat{};
  flat.auraType = kSpellAuraModAttackPower;
  flat.basePoints = 6;
  SpellAuraEffectRow pct{};
  pct.auraType = kSpellAuraModAttackPowerPct;
  pct.basePoints = 6;
  def.auraEffects.push_back(flat);
  def.auraEffects.push_back(pct);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(20572u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 85);

  EXPECT_EQ(bonus.attackPowerModPos, 7);
  EXPECT_FLOAT_EQ(bonus.attackPowerMultiplier, 0.07f);
}

TEST(PlayerAuraStatEffectsTests, ModResistanceAddsSchoolBuffPos) {
  SpellDefinition def{};
  def.id = 20579u;
  def.attributes = SpellAttr0::kPassive;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModResistance;
  row.basePoints = 4;
  row.miscValue = 1 << 5; // Shadow
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(20579u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 85);

  EXPECT_EQ(bonus.resistanceBuffPos[5], 5);
  EXPECT_EQ(bonus.resistanceBuffPos[4], 0);
}

TEST(PlayerAuraStatEffectsTests, ModDamagePercentStacksMultiplicatively) {
  SpellDefinition def{};
  def.id = 28877u;
  def.attributes = SpellAttr0::kPassive;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModDamagePercentDone;
  row.basePoints = 1;
  row.miscValue = 1 << 6; // Arcane
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(28877u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 85);

  EXPECT_FLOAT_EQ(bonus.damageDonePctMultiplier[6], 1.02f);
}

TEST(PlayerAuraStatEffectsTests, ApplyBonusMergesResistanceIntoCombatStats) {
  UnitCombatStats stats{};
  stats.resistance[5] = 10u;
  PlayerAuraStatBonus bonus{};
  bonus.resistanceBuffPos[5] = 5;

  ApplyPlayerAuraStatBonusToCombatStats(stats, bonus);

  EXPECT_EQ(EffectiveSchoolResistance(stats, 5), 15u);
}

TEST(PlayerAuraStatEffectsTests, ModPercentStatUsesPrimaryStatBase) {
  SpellDefinition def{};
  def.id = 20598u;
  def.attributes = SpellAttr0::kPassive;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModPercentStat;
  row.basePoints = 3;
  row.miscValue = 4;
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::array<uint32, 5> prim{20, 20, 20, 20, 100};
  std::vector<Aura> auras{MakePassiveAura(20598u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 85, &prim);

  EXPECT_EQ(bonus.posStat[4], 3);
}

TEST(PlayerAuraStatEffectsTests, MergePermanentPassiveWithoutActiveAura) {
  SpellDefinition def{};
  def.id = 20579u;
  def.attributes = SpellAttr0::kPassive;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModResistance;
  row.basePoints = 4;
  row.miscValue = 1 << 5;
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  PlayerAuraStatBonus bonus{};
  std::vector<uint32_t> passives{20579u};
  std::unordered_set<uint32_t> active{};
  MergePermanentPassiveSpellBonuses(passives, active, &store, 85, nullptr, bonus);
  EXPECT_EQ(bonus.resistanceBuffPos[5], 5);
}

TEST(PlayerAuraStatEffectsTests, BerserkingCombatSpeedSetsHasteMultipliers) {
  SpellDefinition def{};
  def.id = 26297u;
  def.attributes = SpellAttr0::kPassive;
  def.cooldownsId = 1579u;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModCombatSpeedPct;
  row.basePoints = 20;
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(26297u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 85);

  EXPECT_FLOAT_EQ(bonus.meleeHasteMultiplier, 1.21f);
  EXPECT_FLOAT_EQ(bonus.rangedHasteMultiplier, 1.21f);
  EXPECT_FLOAT_EQ(bonus.castHasteMultiplier, 1.21f);
}

TEST(PlayerAuraStatEffectsTests, ModRatingAddsCombatRating) {
  SpellDefinition def{};
  def.id = 200u;
  SpellAuraEffectRow row{};
  row.auraType = kSpellAuraModRating;
  row.basePoints = 7;
  row.miscValue = 5;
  def.auraEffects.push_back(row);

  StatAuraStore store;
  store.Add(def);

  std::vector<Aura> auras{MakePassiveAura(200u)};
  PlayerAuraStatBonus const bonus =
      ComputePlayerAuraStatBonus(auras, &store, 20);

  EXPECT_EQ(bonus.combatRating[5], 8);
}
