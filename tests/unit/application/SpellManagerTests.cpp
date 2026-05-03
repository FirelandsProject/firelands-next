#include <gtest/gtest.h>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <application/ports/IMapCollisionQueries.h>
#include <application/spell/SpellManager.h>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <shared/game/PlayerFactionTeam.h>
#include <shared/game/SpellAttributes.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/WorldOpcodes.h>

using namespace Firelands;

namespace {

class SpellDefinitionStoreMiss final : public ISpellDefinitionStore {
public:
  bool HasSpell(uint32 /*spellId*/) const override { return false; }
  std::optional<SpellDefinition> GetDefinition(uint32 /*spellId*/) const override {
    return std::nullopt;
  }
};

class SpellDefinitionStoreWithCasting final : public ISpellDefinitionStore {
public:
  explicit SpellDefinitionStoreWithCasting(uint32 castingTimeIndex)
      : m_castingTimeIndex(castingTimeIndex) {}

  bool HasSpell(uint32 /*spellId*/) const override { return true; }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    SpellDefinition d{};
    d.id = spellId;
    d.castingTimeIndex = m_castingTimeIndex;
    return d;
  }

private:
  uint32 m_castingTimeIndex;
};

class MockSpellCastTables final : public ISpellCastTables {
public:
  explicit MockSpellCastTables(uint32 castTimeMs, uint32 respondForIndex = 1)
      : m_castTimeMs(castTimeMs), m_respondForIndex(respondForIndex) {}

  uint32 GetCastTimeMs(uint32 castingTimeIndex) const override {
    return castingTimeIndex == m_respondForIndex ? m_castTimeMs : 0u;
  }

  float GetSpellRangeMinYards(uint32 /*rangeIndex*/, bool /*friendlyTarget*/) const override {
    return 0.f;
  }

  float GetSpellRangeMaxYards(uint32 /*rangeIndex*/, bool /*friendlyTarget*/) const override {
    return 0.0f;
  }

  void GetCooldownTiming(uint32 /*cooldownsId*/, uint32 *categoryRecoveryMs,
                         uint32 *recoveryMs, uint32 *startRecoveryMs) const override {
    if (categoryRecoveryMs)
      *categoryRecoveryMs = 0;
    if (recoveryMs)
      *recoveryMs = 0;
    if (startRecoveryMs)
      *startRecoveryMs = 0;
  }

  uint32 GetSpellPowerManaCost(uint32 /*spellPowerId*/) const override { return 0u; }

  uint32 GetSpellCategoryGroupForCategoriesId(uint32 /*categoriesId*/) const override {
    return 0u;
  }

private:
  uint32 m_castTimeMs;
  uint32 m_respondForIndex;
};

class SpellDefinitionWithRange final : public ISpellDefinitionStore {
public:
  explicit SpellDefinitionWithRange(uint32 rangeIndex,
                                    int32 immediateHealthEffectDelta = 0,
                                    uint32 attributes = 0,
                                    uint32 attributesEx = 0,
                                    bool spellEffectHasHealKind = false,
                                    bool spellEffectHasHarmKind = false)
      : m_rangeIndex(rangeIndex),
        m_immediateHealthEffectDelta(immediateHealthEffectDelta),
        m_attributes(attributes),
        m_attributesEx(attributesEx),
        m_spellEffectHasHealKind(spellEffectHasHealKind),
        m_spellEffectHasHarmKind(spellEffectHasHarmKind) {}

  bool HasSpell(uint32 /*spellId*/) const override { return true; }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    SpellDefinition d{};
    d.id = spellId;
    d.rangeIndex = m_rangeIndex;
    d.castingTimeIndex = 1;
    d.immediateHealthEffectDelta = m_immediateHealthEffectDelta;
    d.attributes = m_attributes;
    d.attributesEx = m_attributesEx;
    d.spellEffectHasHealKind = m_spellEffectHasHealKind;
    d.spellEffectHasHarmKind = m_spellEffectHasHarmKind;
    return d;
  }

private:
  uint32 m_rangeIndex;
  int32 m_immediateHealthEffectDelta;
  uint32 m_attributes;
  uint32 m_attributesEx;
  bool m_spellEffectHasHealKind;
  bool m_spellEffectHasHarmKind;
};

class SpellDefinitionWithRangeAndAttr2 final : public ISpellDefinitionStore {
public:
  SpellDefinitionWithRangeAndAttr2(uint32 rangeIndex, uint32 attributesEx2)
      : m_rangeIndex(rangeIndex), m_attributesEx2(attributesEx2) {}

  bool HasSpell(uint32 /*spellId*/) const override { return true; }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    SpellDefinition d{};
    d.id = spellId;
    d.rangeIndex = m_rangeIndex;
    d.castingTimeIndex = 1;
    d.attributesEx2 = m_attributesEx2;
    return d;
  }

private:
  uint32 m_rangeIndex;
  uint32 m_attributesEx2;
};

class MockHostileRangeTables final : public ISpellCastTables {
public:
  MockHostileRangeTables(float hostileMaxYards, uint32 rangeIndex,
                         float hostileMinYards = 0.f, float friendlyMaxYards = -1.f,
                         float friendlyMinYards = -1.f)
      : m_hostileMax(hostileMaxYards),
        m_hostileMin(hostileMinYards),
        m_friendlyMax(friendlyMaxYards >= 0.f ? friendlyMaxYards : hostileMaxYards),
        m_friendlyMin(friendlyMinYards >= 0.f ? friendlyMinYards : hostileMinYards),
        m_rangeIndex(rangeIndex) {}

  uint32 GetCastTimeMs(uint32 /*castingTimeIndex*/) const override { return 0u; }

  float GetSpellRangeMinYards(uint32 rangeIndex, bool friendlyTarget) const override {
    if (rangeIndex != m_rangeIndex)
      return 0.f;
    return friendlyTarget ? m_friendlyMin : m_hostileMin;
  }

  float GetSpellRangeMaxYards(uint32 rangeIndex, bool friendlyTarget) const override {
    if (rangeIndex != m_rangeIndex)
      return 0.f;
    return friendlyTarget ? m_friendlyMax : m_hostileMax;
  }

  void GetCooldownTiming(uint32 /*cooldownsId*/, uint32 *categoryRecoveryMs,
                         uint32 *recoveryMs, uint32 *startRecoveryMs) const override {
    if (categoryRecoveryMs)
      *categoryRecoveryMs = 0;
    if (recoveryMs)
      *recoveryMs = 0;
    if (startRecoveryMs)
      *startRecoveryMs = 0;
  }

  uint32 GetSpellPowerManaCost(uint32 /*spellPowerId*/) const override { return 0u; }

  uint32 GetSpellCategoryGroupForCategoriesId(uint32 /*categoriesId*/) const override {
    return 0u;
  }

private:
  float m_hostileMax;
  float m_hostileMin;
  float m_friendlyMax;
  float m_friendlyMin;
  uint32 m_rangeIndex;
};

class SpellDefinitionWithDirectHealth final : public ISpellDefinitionStore {
public:
  explicit SpellDefinitionWithDirectHealth(int32 healthDelta)
      : m_healthDelta(healthDelta) {}

  bool HasSpell(uint32 /*spellId*/) const override { return true; }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    SpellDefinition d{};
    d.id = spellId;
    d.castingTimeIndex = 1;
    d.rangeIndex = 0;
    d.immediateHealthEffectDelta = m_healthDelta;
    return d;
  }

private:
  int32 m_healthDelta;
};

class CooldownTablesStub final : public ISpellCastTables {
public:
  CooldownTablesStub(uint32 startRecoveryMs, uint32 recoveryMs, uint32 matchCooldownsId)
      : m_start(startRecoveryMs), m_recovery(recoveryMs), m_match(matchCooldownsId) {}

  uint32 GetCastTimeMs(uint32 /*castingTimeIndex*/) const override { return 0u; }

  float GetSpellRangeMinYards(uint32 /*rangeIndex*/, bool /*friendlyTarget*/) const override {
    return 0.f;
  }

  float GetSpellRangeMaxYards(uint32 /*rangeIndex*/, bool /*friendlyTarget*/) const override {
    return 0.f;
  }

  void GetCooldownTiming(uint32 cooldownsId, uint32 *categoryRecoveryMs,
                         uint32 *recoveryMs, uint32 *startRecoveryMs) const override {
    if (categoryRecoveryMs)
      *categoryRecoveryMs = 0;
    if (recoveryMs)
      *recoveryMs = (cooldownsId == m_match) ? m_recovery : 0u;
    if (startRecoveryMs)
      *startRecoveryMs = (cooldownsId == m_match) ? m_start : 0u;
  }

  uint32 GetSpellPowerManaCost(uint32 /*spellPowerId*/) const override { return 0u; }

  uint32 GetSpellCategoryGroupForCategoriesId(uint32 /*categoriesId*/) const override {
    return 0u;
  }

private:
  uint32 m_start;
  uint32 m_recovery;
  uint32 m_match;
};

class CooldownTablesWithCategoryRecovery final : public ISpellCastTables {
public:
  CooldownTablesWithCategoryRecovery(uint32 categoryRecoveryMs, uint32 matchCooldownsId)
      : m_catMs(categoryRecoveryMs), m_match(matchCooldownsId) {}

  uint32 GetCastTimeMs(uint32 /*castingTimeIndex*/) const override { return 0u; }

  float GetSpellRangeMinYards(uint32 /*rangeIndex*/, bool /*friendlyTarget*/) const override {
    return 0.f;
  }

  float GetSpellRangeMaxYards(uint32 /*rangeIndex*/, bool /*friendlyTarget*/) const override {
    return 0.f;
  }

  void GetCooldownTiming(uint32 cooldownsId, uint32 *categoryRecoveryMs,
                         uint32 *recoveryMs, uint32 *startRecoveryMs) const override {
    if (categoryRecoveryMs)
      *categoryRecoveryMs =
          (cooldownsId == m_match) ? m_catMs : 0u;
    if (recoveryMs)
      *recoveryMs = 0;
    if (startRecoveryMs)
      *startRecoveryMs = 0;
  }

  uint32 GetSpellPowerManaCost(uint32 /*spellPowerId*/) const override { return 0u; }

  uint32 GetSpellCategoryGroupForCategoriesId(uint32 categoriesId) const override {
    return categoriesId == 100u ? 7u : 0u;
  }

private:
  uint32 m_catMs;
  uint32 m_match;
};

class SpellDefWithCategoriesAndCooldownsId final : public ISpellDefinitionStore {
public:
  SpellDefWithCategoriesAndCooldownsId(uint32 categoriesId, uint32 cooldownsId)
      : m_categoriesId(categoriesId), m_cooldownsId(cooldownsId) {}

  bool HasSpell(uint32 /*spellId*/) const override { return true; }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    SpellDefinition d{};
    d.id = spellId;
    d.castingTimeIndex = 1;
    d.rangeIndex = 0;
    d.categoriesId = m_categoriesId;
    d.cooldownsId = m_cooldownsId;
    return d;
  }

private:
  uint32 m_categoriesId;
  uint32 m_cooldownsId;
};

class SpellDefWithCooldownsId final : public ISpellDefinitionStore {
public:
  explicit SpellDefWithCooldownsId(uint32 cooldownsId) : m_cdId(cooldownsId) {}

  bool HasSpell(uint32 /*spellId*/) const override { return true; }
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
    SpellDefinition d{};
    d.id = spellId;
    d.castingTimeIndex = 1;
    d.rangeIndex = 0;
    d.cooldownsId = m_cdId;
    return d;
  }

private:
  uint32 m_cdId;
};

class MockCollisionLineOfSight final : public IMapCollisionQueries {
public:
  explicit MockCollisionLineOfSight(bool lineOpen) : m_lineOpen(lineOpen) {}

  bool IsNavMeshDataAvailable(uint32_t /*mapId*/) const override { return true; }

  bool LineOfSight(uint32_t /*mapId*/, float /*x0*/, float /*y0*/, float /*z0*/,
                   float /*x1*/, float /*y1*/, float /*z1*/) const override {
    return m_lineOpen;
  }

private:
  bool m_lineOpen;
};

/// Reads `castTimeMs` from `SMSG_SPELL_START` built by `SpellCastWire::BuildSpellStart`
/// (two packed caster GUIDs, then core fields — no trajectory payload in our wire).
static uint32 ReadSpellStartCastTimeMs(WorldPacket &p) {
  p.SetReadPos(0);
  (void)p.ReadPackedGuid();
  (void)p.ReadPackedGuid();
  (void)p.Read<uint8>();
  (void)p.Read<uint32>();
  (void)p.Read<uint32>();
  (void)p.Read<uint32>();
  return p.Read<uint32>();
}

static uint8 ReadSpellFailureReason(WorldPacket &p) {
  p.SetReadPos(0);
  (void)p.ReadPackedGuid();
  (void)p.Read<uint8>();
  (void)p.Read<int32>();
  return p.Read<uint8>();
}

} // namespace

static SpellCastRequest MakeRequest(uint64 casterGuid, int32 spellId,
                                    std::unordered_set<uint32> *knownSpells) {
  SpellCastRequest req;
  req.casterGuid = casterGuid;
  req.mapId = 0;
  req.client.castId = 7;
  req.client.spellId = spellId;
  req.client.misc = 0;
  req.client.sendCastFlags = 0;
  req.client.targetFlags = 0;
  req.client.unitTargetGuid = 0;
  req.now = std::chrono::steady_clock::now();
  req.gcdReady = {};
  req.knownSpells = knownSpells;
  return req;
}

TEST(SpellManagerTests, UnknownSpell_ReturnsSpellFailure) {
  SpellManager mgr(nullptr);
  std::unordered_set<uint32> known = {100, 200};
  SpellCastRequest req = MakeRequest(0x10ULL, 999, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_TRUE(out.failurePacket.Is(SMSG_SPELL_FAILURE));
}

TEST(SpellManagerTests, GcdActive_ReturnsNotReady) {
  SpellManager mgr(nullptr);
  std::unordered_set<uint32> known = {6673};
  SpellCastRequest req = MakeRequest(0x10ULL, 6673, &known);
  req.gcdReady = req.now + std::chrono::seconds(10);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_TRUE(out.failurePacket.Is(SMSG_SPELL_FAILURE));
}

TEST(SpellManagerTests, KnownSpellOffGcd_ReturnsStartAndGo) {
  SpellManager mgr(nullptr);
  std::unordered_set<uint32> known = {6673};
  SpellCastRequest req = MakeRequest(0xABCDULL, 6673, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
  EXPECT_TRUE(out.spellStart.Is(SMSG_SPELL_START));
  EXPECT_TRUE(out.spellGo.Is(SMSG_SPELL_GO));
  EXPECT_GT(out.newGcdReady, req.now);
}

TEST(SpellManagerTests, NullKnownList_TreatedAsUnknown) {
  SpellManager mgr(nullptr);
  SpellCastRequest req = MakeRequest(0x10ULL, 1, nullptr);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  EXPECT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
}

TEST(SpellManagerTests, DefinitionStoreRejectsKnownSpell) {
  SpellManager mgr(std::make_shared<SpellDefinitionStoreMiss>());
  std::unordered_set<uint32> known = {4242};
  SpellCastRequest req = MakeRequest(0x10ULL, 4242, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_TRUE(out.failurePacket.Is(SMSG_SPELL_FAILURE));
}

TEST(SpellManagerTests, CastTablesFillSpellStartCastTime) {
  auto defs = std::make_shared<SpellDefinitionStoreWithCasting>(9);
  auto tables = std::make_shared<MockSpellCastTables>(3200u, 9u);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartDeferred);
  EXPECT_EQ(ReadSpellStartCastTimeMs(out.spellStart), 3200u);
  EXPECT_EQ(out.deferredCastTimeMs, 3200u);
}

TEST(SpellManagerTests, RangeExceeded_ReturnsOutOfRange) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRange>(kRi);
  auto tables = std::make_shared<MockHostileRangeTables>(5.f, kRi);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 200.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_OUT_OF_RANGE));
}

TEST(SpellManagerTests, RangeWithinMax_ReturnsStartAndGo) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRange>(kRi);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, kRi);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
}

TEST(SpellManagerTests, RangeBelowMin_ReturnsTooClose) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRange>(kRi);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, kRi, 10.f);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 5.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_TOO_CLOSE));
}

TEST(SpellManagerTests, RangeAboveMin_ReturnsStartAndGo) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRange>(kRi);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, kRi, 10.f);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 15.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
}

TEST(SpellManagerTests, FriendlySpellRangeUsesHigherMaxThanHostile) {
  uint32 constexpr kRi = 11;
  auto tables =
      std::make_shared<MockHostileRangeTables>(5.f, kRi, 0.f, 40.f, 0.f);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;

  auto defsDamage = std::make_shared<SpellDefinitionWithRange>(kRi, -1);
  SpellManager mgrDamage(defsDamage, tables);
  SpellCastOutcome outHostile;
  mgrDamage.ProcessCastRequest(req, &outHostile);
  ASSERT_EQ(outHostile.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(outHostile.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_OUT_OF_RANGE));

  auto defsHeal = std::make_shared<SpellDefinitionWithRange>(kRi, 1);
  SpellManager mgrHeal(defsHeal, tables);
  SpellCastOutcome outFriendly;
  mgrHeal.ProcessCastRequest(req, &outFriendly);
  ASSERT_EQ(outFriendly.kind, SpellCastOutcome::Kind::SpellStartAndGo);
}

TEST(SpellManagerTests, BeneficialSpell_FactionHintEnemyTeam_UsesHostileSpellRange) {
  uint32 constexpr kRi = 11;
  auto tables =
      std::make_shared<MockHostileRangeTables>(5.f, kRi, 0.f, 40.f, 0.f);
  auto defsHeal = std::make_shared<SpellDefinitionWithRange>(kRi, 1);
  SpellManager mgrHeal(defsHeal, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasTargetFactionReactionHint = true;
  req.targetIsFriendlyTeamForSpellRange = false;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgrHeal.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_OUT_OF_RANGE));
}

TEST(SpellManagerTests, BeneficialSpell_FactionHintSameTeam_UsesFriendlySpellRange) {
  uint32 constexpr kRi = 11;
  auto tables =
      std::make_shared<MockHostileRangeTables>(5.f, kRi, 0.f, 40.f, 0.f);
  auto defsHeal = std::make_shared<SpellDefinitionWithRange>(kRi, 1);
  SpellManager mgrHeal(defsHeal, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasTargetFactionReactionHint = true;
  req.targetIsFriendlyTeamForSpellRange = true;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgrHeal.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
}

TEST(SpellManagerTests, AuraDebuffAttrUsesHostileRangeOnOtherUnit) {
  uint32 constexpr kRi = 11;
  auto tables =
      std::make_shared<MockHostileRangeTables>(5.f, kRi, 0.f, 40.f, 0.f);
  auto defs = std::make_shared<SpellDefinitionWithRange>(
      kRi, 0, SpellAttr0::kAuraIsDebuff);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_OUT_OF_RANGE));
}

TEST(SpellManagerTests, InitiatesCombatAttrUsesHostileRangeOnOtherUnit) {
  uint32 constexpr kRi = 11;
  auto tables =
      std::make_shared<MockHostileRangeTables>(5.f, kRi, 0.f, 40.f, 0.f);
  auto defs =
      std::make_shared<SpellDefinitionWithRange>(kRi, 0, 0, SpellAttrEx::kInitiatesCombat);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_OUT_OF_RANGE));
}

TEST(SpellManagerTests, SpellEffectHarmKindUsesHostileRangeWhenDeltaZero) {
  uint32 constexpr kRi = 11;
  auto tables =
      std::make_shared<MockHostileRangeTables>(5.f, kRi, 0.f, 40.f, 0.f);
  auto defs = std::make_shared<SpellDefinitionWithRange>(
      kRi, 0, 0, 0, false /*heal*/, true /*harm from SpellEffect.dbc*/);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_OUT_OF_RANGE));
}

TEST(SpellManagerTests, NegativeSpellAttrUsesHostileRangeOnOtherUnit) {
  uint32 constexpr kRi = 11;
  auto tables =
      std::make_shared<MockHostileRangeTables>(5.f, kRi, 0.f, 40.f, 0.f);
  auto defs = std::make_shared<SpellDefinitionWithRange>(
      kRi, 0, SpellAttr0::kNegativeSpell);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_OUT_OF_RANGE));
}

TEST(SpellManagerTests, SpellAttr2IgnoreLineOfSight_BypassesBlockedLos) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRangeAndAttr2>(
      kRi, SpellAttr2::kIgnoreLineOfSight);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, kRi);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  static MockCollisionLineOfSight const kBlocked(false);
  req.collisionQueries = &kBlocked;

  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
}

TEST(SpellManagerTests, LineOfSightBlocked_ReturnsLineOfSightFailure) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRange>(kRi);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, kRi);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  static MockCollisionLineOfSight const kBlocked(false);
  req.collisionQueries = &kBlocked;

  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_LINE_OF_SIGHT));
}

TEST(SpellManagerTests, LineOfSightSkipFlag_IgnoresBlockedLos) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRange>(kRi);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, kRi);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x20ULL;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 10.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  req.skipLineOfSight = true;
  static MockCollisionLineOfSight const kBlocked(false);
  req.collisionQueries = &kBlocked;

  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
}

TEST(SpellManagerTests, GcdUsesSpellCooldownsStartRecovery) {
  auto defs = std::make_shared<SpellDefWithCooldownsId>(42);
  auto tables = std::make_shared<CooldownTablesStub>(3200u, 0u, 42u);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
  auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(out.newGcdReady -
                                                                        req.now)
                      .count();
  EXPECT_GE(ms, 3190);
  EXPECT_LE(ms, 3210);
}

TEST(SpellManagerTests, SufficientMana_SetsPower1Delta) {
  class DefMana final : public ISpellDefinitionStore {
  public:
    bool HasSpell(uint32 /*spellId*/) const override { return true; }
    std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
      SpellDefinition d{};
      d.id = spellId;
      d.castingTimeIndex = 1;
      d.rangeIndex = 0;
      d.manaCost = 30;
      return d;
    }
  };
  auto defs = std::make_shared<DefMana>();
  auto tables = std::make_shared<MockSpellCastTables>(0u);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {555};
  SpellCastRequest req = MakeRequest(0x10ULL, 555, &known);
  req.hasCasterPowerSnapshot = true;
  req.casterPower1 = 100;
  req.casterMaxPower1 = 100;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
  EXPECT_EQ(out.power1Delta, -30);
}

TEST(SpellManagerTests, InsufficientMana_ReturnsNoPower) {
  class DefMana final : public ISpellDefinitionStore {
  public:
    bool HasSpell(uint32 /*spellId*/) const override { return true; }
    std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override {
      SpellDefinition d{};
      d.id = spellId;
      d.castingTimeIndex = 1;
      d.rangeIndex = 0;
      d.manaCost = 100;
      return d;
    }
  };
  auto defs = std::make_shared<DefMana>();
  auto tables = std::make_shared<MockSpellCastTables>(0u);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {555};
  SpellCastRequest req = MakeRequest(0x10ULL, 555, &known);
  req.hasCasterPowerSnapshot = true;
  req.casterPower1 = 50;
  req.casterMaxPower1 = 100;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_NO_POWER));
}

TEST(SpellManagerTests, PerSpellCooldownActive_ReturnsNotReady) {
  auto defs = std::make_shared<SpellDefinitionWithRange>(11);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, 11);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  std::unordered_map<uint32, std::chrono::steady_clock::time_point> cd;
  cd[100u] = req.now + std::chrono::hours(1);
  req.spellCooldownUntilBySpellId = &cd;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_NOT_READY));
}

TEST(SpellManagerTests, CategoryCooldownActive_ReturnsNotReady) {
  auto defs = std::make_shared<SpellDefWithCategoriesAndCooldownsId>(100u, 42u);
  auto tables = std::make_shared<CooldownTablesWithCategoryRecovery>(8000u, 42u);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {200};
  SpellCastRequest req = MakeRequest(0x10ULL, 200, &known);
  std::unordered_map<uint32, std::chrono::steady_clock::time_point> catCd;
  catCd[7u] = req.now + std::chrono::hours(1);
  req.spellCategoryCooldownUntilByGroup = &catCd;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_EQ(ReadSpellFailureReason(out.failurePacket),
            static_cast<uint8>(SpellCastWire::SPELL_FAILED_NOT_READY));
}

TEST(SpellManagerTests, CategoryCooldownOnSuccess_FillsOutcome) {
  auto defs = std::make_shared<SpellDefWithCategoriesAndCooldownsId>(100u, 42u);
  auto tables = std::make_shared<CooldownTablesWithCategoryRecovery>(5000u, 42u);
  SpellManager mgr(defs, tables);
  std::unordered_set<uint32> known = {200};
  SpellCastRequest req = MakeRequest(0x10ULL, 200, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
  EXPECT_EQ(out.spellCategoryCooldownGroup, 7u);
  EXPECT_EQ(out.spellCategoryCooldownDurationMs, 5000u);
}

TEST(SpellManagerTests, DirectHealthEffect_FilledOnSuccess) {
  auto defs = std::make_shared<SpellDefinitionWithDirectHealth>(-42);
  SpellManager mgr(defs, nullptr);
  std::unordered_set<uint32> known = {555};
  SpellCastRequest req = MakeRequest(0x10ULL, 555, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = 0x99ULL;
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
  EXPECT_TRUE(out.hasDirectHealthEffect);
  EXPECT_EQ(out.directHealthTargetGuid, 0x99ULL);
  EXPECT_EQ(out.directHealthDelta, -42);
}

TEST(SpellManagerTests, LineOfSightNotCheckedForSelfTarget) {
  uint32 constexpr kRi = 11;
  auto defs = std::make_shared<SpellDefinitionWithRange>(kRi);
  auto tables = std::make_shared<MockHostileRangeTables>(40.f, kRi);
  SpellManager mgr(defs, tables);
  uint64 constexpr kGuid = 0x10ULL;
  std::unordered_set<uint32> known = {100};
  SpellCastRequest req = MakeRequest(kGuid, 100, &known);
  req.client.targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
  req.client.unitTargetGuid = kGuid;
  req.hasCasterWorldPosition = true;
  req.casterX = 0.f;
  req.casterY = 0.f;
  req.casterZ = 0.f;
  req.hasTargetWorldPosition = true;
  req.targetX = 0.f;
  req.targetY = 0.f;
  req.targetZ = 0.f;
  static MockCollisionLineOfSight const kBlocked(false);
  req.collisionQueries = &kBlocked;

  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
}

TEST(SpellManagerTests, PlayerFactionTeamHint_HumanVsOrc_NotSameTeam) {
  bool same = true;
  ASSERT_TRUE(TrySpellRangeFriendlyTeamHint(1, 2, &same));
  EXPECT_FALSE(same);
}

TEST(SpellManagerTests, PlayerFactionTeamHint_HumanVsDwarf_SameTeam) {
  bool same = false;
  ASSERT_TRUE(TrySpellRangeFriendlyTeamHint(1, 3, &same));
  EXPECT_TRUE(same);
}

TEST(SpellManagerTests, PlayerFactionTeamHint_UnknownRace_ReturnsFalse) {
  bool same = true;
  EXPECT_FALSE(TrySpellRangeFriendlyTeamHint(1, 99, &same));
}
