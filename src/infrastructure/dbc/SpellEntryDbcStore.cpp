#include <infrastructure/dbc/SpellEntryDbcStore.h>

#include <domain/repositories/ISpellCastTables.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Firelands {

namespace {

// TCPP `SpellEffectEntryfmt[] = "nifiiiffiiiiiifiifiiiiiiiix"`
constexpr std::string_view kSpellEffectFmt =
    "nifiiiffiiiiiifiifiiiiiiiix";

constexpr uint32_t kSpellEffectFieldEffect = 1;
constexpr uint32_t kSpellEffectFieldBasePoints = 5;
constexpr uint32_t kSpellEffectFieldDieSides = 9;
constexpr uint32_t kSpellEffectFieldSpellID = 24;
constexpr uint32_t kSpellEffectFieldEffectIndex = 25;

constexpr uint32_t SPELL_EFFECT_SCHOOL_DAMAGE = 2;
constexpr uint32_t SPELL_EFFECT_HEAL = 10;

// TCPP `src/server/game/DataStores/DBCfmt.h` — must match client `Spell.dbc` for 15595.
constexpr std::string_view kSpellEntryFmt =
    "niiiiiiiiiiiiiiifiiiissxxiixxifiiiiiiixiiiiiiiii";

// Field indices = character index in `kSpellEntryFmt` (TCPP `SpellEntry` order).
constexpr uint32_t kFieldId = 0;
constexpr uint32_t kFieldAttributes = 1;
constexpr uint32_t kFieldAttributesEx = 2;
constexpr uint32_t kFieldAttributesEx2 = 3;
constexpr uint32_t kFieldCastingTimeIndex = 12;
constexpr uint32_t kFieldDurationIndex = 13;
constexpr uint32_t kFieldPowerType = 14;
constexpr uint32_t kFieldRangeIndex = 15;
constexpr uint32_t kFieldSchoolMask = 25;
constexpr uint32_t kFieldCategoriesId = 35;
constexpr uint32_t kFieldCooldownsId = 37;
/// TCPP `SpellEntry::PowerDisplayID` — row id in `SpellPower.dbc` (`SpellInfo::SpellPowerId`).
constexpr uint32_t kFieldSpellPowerId = 42;

static size_t LastFieldSizeBytes(char lastFmt) {
  return ((lastFmt == 'b') || (lastFmt == 'X')) ? 1u : 4u;
}

static bool ExpectedRecordLayout(DbcReader const &reader, std::string_view fmt,
                                 std::vector<uint32_t> const &offsets) {
  if (offsets.size() != reader.GetFieldCount())
    return false;
  if (offsets.empty())
    return false;
  char const last = fmt[fmt.size() - 1];
  size_t const expected =
      static_cast<size_t>(offsets.back()) + LastFieldSizeBytes(last);
  return expected == static_cast<size_t>(reader.GetRecordSize());
}

static char const* const kSpellDbcMergeQueryFull =
    "SELECT `Id`, `Attributes`, `AttributesEx`, `AttributesEx2`, `CastingTimeIndex`, "
    "`DurationIndex`, "
    "`RangeIndex`, "
    "`SchoolMask`, `PowerType`, `OvAttributes`, `OvCastingTimeIndex`, `OvDurationIndex`, "
    "`OvRangeIndex`, `OvSchoolMask` FROM `firelands_world`.`spell_dbc`";

static char const* const kSpellDbcMergeQueryLegacy =
    "SELECT `Id`, `Attributes`, `AttributesEx`, `AttributesEx2`, `CastingTimeIndex`, "
    "`DurationIndex`, "
    "`RangeIndex`, "
    "`SchoolMask`, `PowerType` FROM `firelands_world`.`spell_dbc`";

static void ApplySpellDbcMergeRow(sql::ResultSet& rs, bool hasOvColumns,
                                  std::unordered_map<uint32, SpellDefinition>& byId) {
  uint32 const id = static_cast<uint32>(rs.getUInt("Id"));
  if (id == 0u)
    return;

  uint32 const attributes = static_cast<uint32>(rs.getUInt("Attributes"));
  uint32 const castingTimeIndex =
      static_cast<uint32>(rs.getUInt("CastingTimeIndex"));
  uint32 const durationIndex = static_cast<uint32>(rs.getUInt("DurationIndex"));
  uint32 const rangeIndex = static_cast<uint32>(rs.getUInt("RangeIndex"));
  uint32 const schoolMask = static_cast<uint32>(rs.getUInt("SchoolMask"));
  bool const hasPowerOverride = !rs.isNull("PowerType");
  uint32 const powerOverride =
      hasPowerOverride ? static_cast<uint32>(rs.getUInt("PowerType")) : 0u;

  auto it = byId.find(id);
  if (it != byId.end()) {
    SpellDefinition& d = it->second;
    if (hasPowerOverride)
      d.powerType = powerOverride;
    if (hasOvColumns) {
      if (!rs.isNull("OvAttributes"))
        d.attributes = static_cast<uint32>(rs.getUInt("OvAttributes"));
      if (!rs.isNull("OvCastingTimeIndex"))
        d.castingTimeIndex = static_cast<uint32>(rs.getUInt("OvCastingTimeIndex"));
      if (!rs.isNull("OvDurationIndex"))
        d.durationIndex = static_cast<uint32>(rs.getUInt("OvDurationIndex"));
      if (!rs.isNull("OvRangeIndex"))
        d.rangeIndex = static_cast<uint32>(rs.getUInt("OvRangeIndex"));
      if (!rs.isNull("OvSchoolMask"))
        d.schoolMask = static_cast<uint32>(rs.getUInt("OvSchoolMask"));
    }
    return;
  }

  uint32 const attributesEx = static_cast<uint32>(rs.getUInt("AttributesEx"));
  uint32 const attributesEx2 = static_cast<uint32>(rs.getUInt("AttributesEx2"));
  SpellDefinition d{};
  d.id = id;
  d.attributes = attributes;
  d.attributesEx = attributesEx;
  d.attributesEx2 = attributesEx2;
  d.castingTimeIndex = castingTimeIndex;
  d.durationIndex = durationIndex;
  d.rangeIndex = rangeIndex;
  d.schoolMask = schoolMask;
  d.powerType = hasPowerOverride ? powerOverride : 0u;
  if (hasOvColumns) {
    if (!rs.isNull("OvAttributes"))
      d.attributes = static_cast<uint32>(rs.getUInt("OvAttributes"));
    if (!rs.isNull("OvCastingTimeIndex"))
      d.castingTimeIndex = static_cast<uint32>(rs.getUInt("OvCastingTimeIndex"));
    if (!rs.isNull("OvDurationIndex"))
      d.durationIndex = static_cast<uint32>(rs.getUInt("OvDurationIndex"));
    if (!rs.isNull("OvRangeIndex"))
      d.rangeIndex = static_cast<uint32>(rs.getUInt("OvRangeIndex"));
    if (!rs.isNull("OvSchoolMask"))
      d.schoolMask = static_cast<uint32>(rs.getUInt("OvSchoolMask"));
  }
  byId.emplace(id, d);
}

static int32 SignedImmediateHealthDelta(uint32 effect, int32 basePoints,
                                        int32 dieSides) {
  int32 const bpPlusOne = basePoints + 1;
  int32 diceMid = 0;
  if (dieSides > 0)
    diceMid = (dieSides + 1) / 2;
  int32 const magnitude = bpPlusOne + diceMid;
  if (magnitude == 0)
    return 0;
  if (effect == SPELL_EFFECT_SCHOOL_DAMAGE)
    return -magnitude;
  if (effect == SPELL_EFFECT_HEAL)
    return magnitude;
  return 0;
}

} // namespace

bool SpellEntryDbcStore::Load(std::string const &path) {
  m_loaded = false;
  m_byId.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kSpellEntryFmt);
  if (!reader.VerifyFormat(kSpellEntryFmt)) {
    LOG_WARN("Spell.dbc: field count {} does not match SpellEntryfmt length {}",
             reader.GetFieldCount(), kSpellEntryFmt.size());
    return false;
  }
  if (!ExpectedRecordLayout(reader, kSpellEntryFmt, offsets)) {
    LOG_WARN(
        "Spell.dbc: record size {} does not match SpellEntryfmt-derived size (path={})",
        reader.GetRecordSize(), path);
    return false;
  }

  uint32_t const n = reader.GetRecordCount();
  m_byId.reserve(static_cast<size_t>(n));
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, kFieldId, offsets);
    if (id == 0u)
      continue;
    SpellDefinition def;
    def.id = id;
    def.attributes = reader.ReadUInt32(rec, kFieldAttributes, offsets);
    def.attributesEx = reader.ReadUInt32(rec, kFieldAttributesEx, offsets);
    def.attributesEx2 = reader.ReadUInt32(rec, kFieldAttributesEx2, offsets);
    def.castingTimeIndex = reader.ReadUInt32(rec, kFieldCastingTimeIndex, offsets);
    def.durationIndex = reader.ReadUInt32(rec, kFieldDurationIndex, offsets);
    def.powerType = reader.ReadUInt32(rec, kFieldPowerType, offsets);
    def.rangeIndex = reader.ReadUInt32(rec, kFieldRangeIndex, offsets);
    def.schoolMask = reader.ReadUInt32(rec, kFieldSchoolMask, offsets);
    def.categoriesId = reader.ReadUInt32(rec, kFieldCategoriesId, offsets);
    def.cooldownsId = reader.ReadUInt32(rec, kFieldCooldownsId, offsets);
    def.spellPowerId = reader.ReadUInt32(rec, kFieldSpellPowerId, offsets);
    m_byId.emplace(id, def);
  }

  m_loaded = true;
  LOG_DEBUG("Spell.dbc: {} spell definitions from {}.", m_byId.size(), path);
  return true;
}

void SpellEntryDbcStore::ApplySpellPowerManaFromTables(ISpellCastTables const &tables) {
  for (auto &kv : m_byId) {
    SpellDefinition &d = kv.second;
    if (d.spellPowerId == 0u)
      continue;
    d.manaCost = tables.GetSpellPowerManaCost(d.spellPowerId);
  }
}

void SpellEntryDbcStore::MergeSpellDbcRows(std::shared_ptr<sql::Connection> worldConn) {
  if (!worldConn)
    return;
  try {
    std::unique_ptr<sql::Statement> st(worldConn->createStatement());
    std::unique_ptr<sql::ResultSet> rs;
    bool hasOvColumns = true;
    try {
      rs.reset(st->executeQuery(kSpellDbcMergeQueryFull));
    } catch (sql::SQLException& e) {
      if (e.getErrorCode() != 1054) {
        throw;
      }
      hasOvColumns = false;
      LOG_DEBUG(
          "spell_dbc: Ov* columns missing (apply migration 18); merge uses PowerType "
          "and full-row custom spells only.");
      rs.reset(st->executeQuery(kSpellDbcMergeQueryLegacy));
    }

    size_t merged = 0;
    while (rs->next()) {
      ApplySpellDbcMergeRow(*rs, hasOvColumns, m_byId);
      ++merged;
    }
    if (merged > 0u)
      LOG_INFO("Merged {} `spell_dbc` row(s) over Spell.dbc (world DB).", merged);
  } catch (sql::SQLException& e) {
    if (e.getErrorCode() == 1146) {
      LOG_WARN(
          "`firelands_world.spell_dbc` missing (apply world migrations); using DBC "
          "only.");
    } else {
      LOG_WARN("spell_dbc merge skipped: {}", e.what());
    }
  }
}

void SpellEntryDbcStore::MergeImmediateHealthFromSpellEffect(
    std::string const &path) {
  if (m_byId.empty())
    return;

  DbcReader reader;
  if (!reader.Load(path)) {
    LOG_WARN("SpellEffect.dbc: failed to load {}.", path);
    return;
  }

  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kSpellEffectFmt);
  if (!reader.VerifyFormat(kSpellEffectFmt)) {
    LOG_WARN("SpellEffect.dbc: field count {} does not match format length {} (path={}).",
             reader.GetFieldCount(), kSpellEffectFmt.size(), path);
    return;
  }
  if (!ExpectedRecordLayout(reader, kSpellEffectFmt, offsets)) {
    LOG_WARN(
        "SpellEffect.dbc: record size {} does not match format-derived size (path={}).",
        reader.GetRecordSize(), path);
    return;
  }

  struct CandidateRow {
    uint32_t spellId;
    uint32_t effectIndex;
    uint32_t effect;
    int32_t basePoints;
    int32_t dieSides;
  };
  std::vector<CandidateRow> candidates;
  uint32_t const n = reader.GetRecordCount();
  candidates.reserve(static_cast<size_t>(n) / 8u + 8u);

  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const spellId =
        reader.ReadUInt32(rec, kSpellEffectFieldSpellID, offsets);
    if (spellId == 0u)
      continue;
    uint32_t const effect =
        reader.ReadUInt32(rec, kSpellEffectFieldEffect, offsets);
    if (effect != SPELL_EFFECT_SCHOOL_DAMAGE && effect != SPELL_EFFECT_HEAL)
      continue;
    int32_t const basePoints =
        reader.ReadInt32(rec, kSpellEffectFieldBasePoints, offsets);
    int32_t const dieSides =
        reader.ReadInt32(rec, kSpellEffectFieldDieSides, offsets);
    uint32_t const effectIndex =
        reader.ReadUInt32(rec, kSpellEffectFieldEffectIndex, offsets);
    candidates.push_back(CandidateRow{spellId, effectIndex, effect, basePoints,
                                      dieSides});
  }

  std::sort(candidates.begin(), candidates.end(),
            [](CandidateRow const &a, CandidateRow const &b) {
              if (a.spellId != b.spellId)
                return a.spellId < b.spellId;
              return a.effectIndex < b.effectIndex;
            });

  size_t applied = 0;
  uint32_t skipSpellId = 0;
  for (CandidateRow const &row : candidates) {
    if (row.spellId == skipSpellId)
      continue;
    auto it = m_byId.find(row.spellId);
    if (it == m_byId.end()) {
      skipSpellId = row.spellId;
      continue;
    }
    skipSpellId = row.spellId;
    int32_t const delta =
        SignedImmediateHealthDelta(row.effect, row.basePoints, row.dieSides);
    if (delta != 0) {
      it->second.immediateHealthEffectDelta = delta;
      ++applied;
    }
  }

  if (applied > 0u)
    LOG_DEBUG(
        "SpellEffect.dbc: set immediateHealthEffectDelta on {} spell(s) from {}.",
        applied, path);
}

bool SpellEntryDbcStore::HasSpell(uint32 spellId) const {
  if (spellId == 0u)
    return false;
  return m_byId.find(spellId) != m_byId.end();
}

std::optional<SpellDefinition> SpellEntryDbcStore::GetDefinition(uint32 spellId) const {
  if (spellId == 0u)
    return std::nullopt;
  auto it = m_byId.find(spellId);
  if (it == m_byId.end())
    return std::nullopt;
  return it->second;
}

} // namespace Firelands
