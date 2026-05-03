#include <shared/dbc/GtPlayerStatGameTables.h>

#include <shared/Logger.h>
#include <shared/dbc/DbcReader.h>

namespace Firelands {

namespace {

std::string NormalizeDbcDir(std::string const &dbcDirectory) {
  if (dbcDirectory.empty())
    return {};
  return dbcDirectory.back() == '/' ? dbcDirectory : (dbcDirectory + "/");
}

bool LoadIdFloatDbc(DbcReader &out, bool &flag, std::string const &path) {
  if (!out.Load(path)) {
    flag = false;
    return false;
  }
  if (out.GetRecordCount() == 0u || out.GetRecordSize() != 8u ||
      out.GetFieldCount() != 2u) {
    LOG_WARN(
        "GtPlayerStatGameTables: unexpected layout in {} (records={}, fields={}, "
        "recordSize={})",
        path, out.GetRecordCount(), out.GetFieldCount(), out.GetRecordSize());
    flag = false;
    return false;
  }
  flag = true;
  return true;
}

} // namespace

float GtPlayerStatGameTables::ReadIdFloatPair(
    DbcReader const &reader, uint32_t recordIndex,
    std::vector<uint32_t> const &offsets) {
  return reader.ReadFloat(recordIndex, 1u, offsets);
}

bool GtPlayerStatGameTables::Load(std::string const &dbcDirectory) {
  m_combatRatingsLoaded = m_meleeCritLoaded = m_spellCritLoaded =
      m_spellCritBaseLoaded = false;
  m_offIf = DbcBuildFieldByteOffsets("if");
  if (dbcDirectory.empty())
    return false;
  std::string const base = NormalizeDbcDir(dbcDirectory);

  bool any = false;
  if (LoadIdFloatDbc(m_combatRatings, m_combatRatingsLoaded,
                     base + "gtCombatRatings.dbc"))
    any = true;
  else
    LOG_WARN("GtPlayerStatGameTables: failed to load {}", base + "gtCombatRatings.dbc");

  if (!LoadIdFloatDbc(m_meleeCrit, m_meleeCritLoaded,
                      base + "gtChanceToMeleeCrit.dbc"))
    LOG_WARN("GtPlayerStatGameTables: failed to load {}",
             base + "gtChanceToMeleeCrit.dbc");
  else
    any = true;

  if (!LoadIdFloatDbc(m_spellCrit, m_spellCritLoaded,
                      base + "gtChanceToSpellCrit.dbc"))
    LOG_WARN("GtPlayerStatGameTables: failed to load {}",
             base + "gtChanceToSpellCrit.dbc");
  else
    any = true;

  if (!LoadIdFloatDbc(m_spellCritBase, m_spellCritBaseLoaded,
                      base + "gtChanceToSpellCritBase.dbc"))
    LOG_WARN("GtPlayerStatGameTables: failed to load {}",
             base + "gtChanceToSpellCritBase.dbc");
  else
    any = true;

  if (m_combatRatingsLoaded)
    LOG_DEBUG("GtPlayerStatGameTables: gtCombatRatings.dbc ({} rows).",
             m_combatRatings.GetRecordCount());
  return any;
}

float GtPlayerStatGameTables::CombatRatingPerPercent(uint8_t combatRatingIndex,
                                                     uint8_t level) const {
  if (!m_combatRatingsLoaded || level == 0u)
    return 0.f;
  uint32_t const lvl = static_cast<uint32_t>(std::min<uint8_t>(level, 100u));
  uint32_t const record =
      static_cast<uint32_t>(combatRatingIndex) * 100u + (lvl - 1u);
  if (record >= m_combatRatings.GetRecordCount())
    return 0.f;
  float const v = ReadIdFloatPair(m_combatRatings, record, m_offIf);
  return v > 0.f ? v : 0.f;
}

float GtPlayerStatGameTables::ChanceToMeleeCrit(uint8_t classId,
                                                uint8_t level) const {
  if (!m_meleeCritLoaded || classId == 0u || classId > 11u || level == 0u)
    return 0.f;
  uint32_t const lvl = static_cast<uint32_t>(std::min<uint8_t>(level, 100u));
  uint32_t const record =
      (static_cast<uint32_t>(classId) - 1u) * 100u + (lvl - 1u);
  if (record >= m_meleeCrit.GetRecordCount())
    return 0.f;
  return ReadIdFloatPair(m_meleeCrit, record, m_offIf);
}

float GtPlayerStatGameTables::ChanceToSpellCrit(uint8_t classId,
                                                uint8_t level) const {
  if (!m_spellCritLoaded || classId == 0u || classId > 11u || level == 0u)
    return 0.f;
  uint32_t const lvl = static_cast<uint32_t>(std::min<uint8_t>(level, 100u));
  uint32_t const record =
      (static_cast<uint32_t>(classId) - 1u) * 100u + (lvl - 1u);
  if (record >= m_spellCrit.GetRecordCount())
    return 0.f;
  return ReadIdFloatPair(m_spellCrit, record, m_offIf);
}

float GtPlayerStatGameTables::ChanceToSpellCritBase(uint8_t classId) const {
  if (!m_spellCritBaseLoaded || classId == 0u || classId > 11u)
    return 0.f;
  uint32_t const record = static_cast<uint32_t>(classId) - 1u;
  if (record >= m_spellCritBase.GetRecordCount())
    return 0.f;
  return ReadIdFloatPair(m_spellCritBase, record, m_offIf);
}

} // namespace Firelands
