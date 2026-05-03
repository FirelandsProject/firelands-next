#include <shared/dbc/SpellDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

namespace Firelands {

bool SpellDbc::Load(std::string const &path) {
  m_loaded = false;
  m_spellIds.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  uint32_t const recordCount = reader.GetRecordCount();
  uint32_t const recordSize = reader.GetRecordSize();
  if (recordCount == 0u || recordSize < sizeof(uint32_t)) {
    LOG_WARN("Spell.dbc: no records or record size too small ({})", path);
    return false;
  }

  m_spellIds.reserve(static_cast<size_t>(recordCount));
  for (uint32_t rec = 0; rec < recordCount; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0);
    if (id != 0u)
      m_spellIds.insert(id);
  }

  m_loaded = true;
  LOG_DEBUG("Spell.dbc: {} spell ids from {}.", m_spellIds.size(), path);
  return true;
}

bool SpellDbc::HasSpell(uint32_t spellId) const {
  if (!m_loaded || spellId == 0u)
    return true;
  return m_spellIds.find(spellId) != m_spellIds.end();
}

} // namespace Firelands
