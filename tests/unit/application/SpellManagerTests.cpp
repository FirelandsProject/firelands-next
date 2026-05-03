#include <gtest/gtest.h>
#include <application/spell/SpellManager.h>
#include <domain/repositories/ISpellCastTables.h>
#include <domain/repositories/ISpellDefinitionStore.h>
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

  float GetHostileRangeMaxYards(uint32 /*rangeIndex*/) const override {
    return 0.0f;
  }

private:
  uint32 m_castTimeMs;
  uint32 m_respondForIndex;
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

} // namespace

static SpellCastRequest MakeRequest(uint64 casterGuid, int32 spellId,
                                    std::vector<uint32> *knownSpells) {
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
  std::vector<uint32> known = {100, 200};
  SpellCastRequest req = MakeRequest(0x10ULL, 999, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_TRUE(out.failurePacket.Is(SMSG_SPELL_FAILURE));
}

TEST(SpellManagerTests, GcdActive_ReturnsNotReady) {
  SpellManager mgr(nullptr);
  std::vector<uint32> known = {6673};
  SpellCastRequest req = MakeRequest(0x10ULL, 6673, &known);
  req.gcdReady = req.now + std::chrono::seconds(10);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellFailure);
  EXPECT_TRUE(out.failurePacket.Is(SMSG_SPELL_FAILURE));
}

TEST(SpellManagerTests, KnownSpellOffGcd_ReturnsStartAndGo) {
  SpellManager mgr(nullptr);
  std::vector<uint32> known = {6673};
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
  std::vector<uint32> known = {4242};
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
  std::vector<uint32> known = {100};
  SpellCastRequest req = MakeRequest(0x10ULL, 100, &known);
  SpellCastOutcome out;
  mgr.ProcessCastRequest(req, &out);
  ASSERT_EQ(out.kind, SpellCastOutcome::Kind::SpellStartAndGo);
  EXPECT_EQ(ReadSpellStartCastTimeMs(out.spellStart), 3200u);
}
