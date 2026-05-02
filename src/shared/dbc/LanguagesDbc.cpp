#include <shared/dbc/LanguagesDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

namespace Firelands {

bool LanguagesDbc::Load(std::string const &path) {
  m_loaded = false;
  m_languageIds.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  uint32_t const recordCount = reader.GetRecordCount();
  uint32_t const recordSize = reader.GetRecordSize();
  if (recordCount == 0u || recordSize < sizeof(uint32_t)) {
    LOG_WARN("Languages.dbc: no records or record size too small ({})", path);
    return false;
  }

  // 15595 extracted client: fieldCount=2, recordSize=8 → uint32 id + uint32 name
  // string offset. Older multi-locale layouts use larger rows; we only need id.
  if (reader.GetFieldCount() != 2u || recordSize != 8u) {
    LOG_WARN(
        "Languages.dbc: unexpected header (fields={}, recordSize={}) in {}; "
        "reading first uint32 per row anyway.",
        reader.GetFieldCount(), recordSize, path);
  }

  for (uint32_t rec = 0; rec < recordCount; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0);
    if (id != 0u)
      m_languageIds.insert(id);
  }

  m_loaded = true;
  LOG_INFO("Languages.dbc: {} language ids from {}.", m_languageIds.size(), path);
  return true;
}

bool LanguagesDbc::HasLanguageId(uint32_t languageId) const {
  if (!m_loaded)
    return true;
  return m_languageIds.find(languageId) != m_languageIds.end();
}

} // namespace Firelands
