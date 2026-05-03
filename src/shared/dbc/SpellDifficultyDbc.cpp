#include <shared/dbc/SpellDifficultyDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

namespace Firelands {

bool SpellDifficultyDbc::Load(std::string const &path) {
  m_loaded = false;
  m_rows.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  uint32_t const recordCount = reader.GetRecordCount();
  uint32_t const fieldCount = reader.GetFieldCount();
  uint32_t const recordSize = reader.GetRecordSize();
  // "niiii": five 32-bit fields (DBCfmt.h).
  if (recordCount == 0u || fieldCount < 5u || recordSize < 5u * sizeof(uint32_t)) {
    LOG_WARN("SpellDifficulty.dbc: unexpected layout (records={}, fields={}, "
             "recordSize={}) in {}",
             recordCount, fieldCount, recordSize, path);
    return false;
  }

  m_rows.reserve(static_cast<size_t>(recordCount));
  for (uint32_t rec = 0; rec < recordCount; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0);
    if (id == 0u)
      continue;
    std::array<uint32_t, 4> spells{};
    for (unsigned i = 0; i < 4; ++i)
      spells[i] = reader.ReadUInt32(rec, 1u + i);
    m_rows.emplace(id, spells);
  }

  m_loaded = true;
  LOG_DEBUG("SpellDifficulty.dbc: {} entries from {}.", m_rows.size(), path);
  return true;
}

uint32_t SpellDifficultyDbc::GetSpellId(uint32_t difficultyId,
                                        unsigned index) const {
  if (!m_loaded || index > 3u)
    return 0u;
  auto it = m_rows.find(difficultyId);
  if (it == m_rows.end())
    return 0u;
  return it->second[index];
}

} // namespace Firelands
