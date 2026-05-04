#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <string_view>
#include <vector>

namespace Firelands {

namespace {

// Trinity Cataclysm 4.3.4 `FactionTemplateEntryfmt[]` (see TCPP `DBCfmt.h`).
constexpr std::string_view kFactionTemplateFmt = "niiiiiiiiiiiii";

static size_t LastFieldSizeBytes(char lastFmt) {
  return ((lastFmt == 'b') || (lastFmt == 'X')) ? 1u : 4u;
}

static bool RecordLayoutMatches(DbcReader const &reader,
                                std::string_view fmt,
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

} // namespace

bool FactionTemplateDbc::Load(std::string const &path) {
  m_loaded = false;
  m_byId.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kFactionTemplateFmt);
  if (!reader.VerifyFormat(kFactionTemplateFmt) ||
      !RecordLayoutMatches(reader, kFactionTemplateFmt, offsets)) {
    LOG_WARN("FactionTemplate.dbc layout mismatch (expected fmt {}, recordSize {} fields "
             "{}) in {}",
             kFactionTemplateFmt, reader.GetRecordSize(), reader.GetFieldCount(), path);
    return false;
  }

  for (uint32_t ri = 0; ri < reader.GetRecordCount(); ++ri) {
    FactionTemplateEntry row;
    row.id = reader.ReadUInt32(ri, 0, offsets);
    if (row.id == 0)
      continue;
    row.faction = reader.ReadUInt32(ri, 1, offsets);
    row.flags = reader.ReadUInt32(ri, 2, offsets);
    row.factionGroup = reader.ReadUInt32(ri, 3, offsets);
    row.friendGroup = reader.ReadUInt32(ri, 4, offsets);
    row.enemyGroup = reader.ReadUInt32(ri, 5, offsets);
    for (int i = 0; i < 4; ++i)
      row.enemies[i] = reader.ReadUInt32(ri, static_cast<uint32_t>(6 + i), offsets);
    for (int i = 0; i < 4; ++i)
      row.friendFactions[i] = reader.ReadUInt32(ri, static_cast<uint32_t>(10 + i), offsets);
    m_byId[row.id] = row;
  }

  m_loaded = true;
  LOG_INFO("FactionTemplate.dbc: loaded {} template(s) from {}", m_byId.size(), path);
  return true;
}

bool FactionTemplateDbc::HasEntry(uint32_t templateId) const {
  if (!m_loaded || templateId == 0)
    return false;
  return m_byId.find(templateId) != m_byId.end();
}

std::optional<FactionTemplateEntry> FactionTemplateDbc::TryGet(uint32_t templateId) const {
  if (!m_loaded || templateId == 0)
    return std::nullopt;
  auto it = m_byId.find(templateId);
  if (it == m_byId.end())
    return std::nullopt;
  return it->second;
}

} // namespace Firelands
