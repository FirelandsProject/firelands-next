#include <infrastructure/dbc/SpellEntryDbcStore.h>

#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <cstddef>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace Firelands {

namespace {

// TCPP `src/server/game/DataStores/DBCfmt.h` — must match client `Spell.dbc` for 15595.
constexpr std::string_view kSpellEntryFmt =
    "niiiiiiiiiiiiiiifiiiissxxiixxifiiiiiiixiiiiiiiii";

// Field indices = character index in `kSpellEntryFmt` (TCPP `SpellEntry` order).
constexpr uint32_t kFieldId = 0;
constexpr uint32_t kFieldAttributes = 1;
constexpr uint32_t kFieldCastingTimeIndex = 12;
constexpr uint32_t kFieldDurationIndex = 13;
constexpr uint32_t kFieldPowerType = 14;
constexpr uint32_t kFieldRangeIndex = 15;
constexpr uint32_t kFieldSchoolMask = 25;

static size_t LastFieldSizeBytes(char lastFmt) {
  return ((lastFmt == 'b') || (lastFmt == 'X')) ? 1u : 4u;
}

static bool ExpectedLayout(DbcReader const &reader,
                           std::vector<uint32_t> const &offsets) {
  if (offsets.size() != reader.GetFieldCount())
    return false;
  if (offsets.empty())
    return false;
  char const last = kSpellEntryFmt[kSpellEntryFmt.size() - 1];
  size_t const expected =
      static_cast<size_t>(offsets.back()) + LastFieldSizeBytes(last);
  return expected == static_cast<size_t>(reader.GetRecordSize());
}

static char const* const kSpellDbcMergeQueryFull =
    "SELECT `Id`, `Attributes`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, "
    "`SchoolMask`, `PowerType`, `OvAttributes`, `OvCastingTimeIndex`, `OvDurationIndex`, "
    "`OvRangeIndex`, `OvSchoolMask` FROM `firelands_world`.`spell_dbc`";

static char const* const kSpellDbcMergeQueryFullWithMvp =
    "SELECT `Id`, `Attributes`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, "
    "`SchoolMask`, `PowerType`, `OvAttributes`, `OvCastingTimeIndex`, `OvDurationIndex`, "
    "`OvRangeIndex`, `OvSchoolMask`, `MvpDirectHealthDelta` FROM "
    "`firelands_world`.`spell_dbc`";

static char const* const kSpellDbcMergeQueryLegacy =
    "SELECT `Id`, `Attributes`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, "
    "`SchoolMask`, `PowerType` FROM `firelands_world`.`spell_dbc`";

static void ApplySpellDbcMergeRow(sql::ResultSet& rs, bool hasOvColumns,
                                  bool hasMvpHealthColumn,
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
    if (hasMvpHealthColumn && !rs.isNull("MvpDirectHealthDelta"))
      d.directHealthEffectBasePoints =
          static_cast<int32>(rs.getInt("MvpDirectHealthDelta"));
    return;
  }

  SpellDefinition d{};
  d.id = id;
  d.attributes = attributes;
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
  if (hasMvpHealthColumn && !rs.isNull("MvpDirectHealthDelta"))
    d.directHealthEffectBasePoints =
        static_cast<int32>(rs.getInt("MvpDirectHealthDelta"));
  byId.emplace(id, d);
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
  if (!ExpectedLayout(reader, offsets)) {
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
    def.castingTimeIndex = reader.ReadUInt32(rec, kFieldCastingTimeIndex, offsets);
    def.durationIndex = reader.ReadUInt32(rec, kFieldDurationIndex, offsets);
    def.powerType = reader.ReadUInt32(rec, kFieldPowerType, offsets);
    def.rangeIndex = reader.ReadUInt32(rec, kFieldRangeIndex, offsets);
    def.schoolMask = reader.ReadUInt32(rec, kFieldSchoolMask, offsets);
    m_byId.emplace(id, def);
  }

  m_loaded = true;
  LOG_DEBUG("Spell.dbc: {} spell definitions from {}.", m_byId.size(), path);
  return true;
}

void SpellEntryDbcStore::MergeSpellDbcRows(std::shared_ptr<sql::Connection> worldConn) {
  if (!worldConn)
    return;
  try {
    std::unique_ptr<sql::Statement> st(worldConn->createStatement());
    std::unique_ptr<sql::ResultSet> rs;
    bool hasOvColumns = true;
    bool hasMvpHealthColumn = false;
    try {
      rs.reset(st->executeQuery(kSpellDbcMergeQueryFullWithMvp));
      hasMvpHealthColumn = true;
    } catch (sql::SQLException& e) {
      if (e.getErrorCode() != 1054) {
        throw;
      }
      try {
        rs.reset(st->executeQuery(kSpellDbcMergeQueryFull));
      } catch (sql::SQLException& e2) {
        if (e2.getErrorCode() != 1054) {
          throw;
        }
        hasOvColumns = false;
        LOG_DEBUG(
            "spell_dbc: Ov* columns missing (apply migration 18); merge uses PowerType "
            "and full-row custom spells only.");
        rs.reset(st->executeQuery(kSpellDbcMergeQueryLegacy));
      }
    }

    size_t merged = 0;
    while (rs->next()) {
      ApplySpellDbcMergeRow(*rs, hasOvColumns, hasMvpHealthColumn, m_byId);
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
