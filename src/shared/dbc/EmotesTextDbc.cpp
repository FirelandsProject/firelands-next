#include <shared/dbc/EmotesTextDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

namespace Firelands {

bool EmotesTextDbc::Load(std::string const &path) {
  m_loaded = false;
  m_textEmoteToAnim.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  uint32_t const recordCount = reader.GetRecordCount();
  uint32_t const recordSize = reader.GetRecordSize();
  if (recordCount == 0u || recordSize < 12u) {
    LOG_WARN("EmotesText.dbc: no records or record size too small ({})", path);
    return false;
  }

  // `EmotesTextEntryfmt`: "nxixxxxxxxxxxxxxxxx"
  if (!reader.VerifyFormat("nxixxxxxxxxxxxxxxxx")) {
    LOG_WARN("EmotesText.dbc: unexpected field count {} in {} (expected 19)",
             reader.GetFieldCount(), path);
  }

  auto const fieldOffsets = DbcBuildFieldByteOffsets("nxixxxxxxxxxxxxxxxx");
  if (fieldOffsets.size() < 3u) {
    LOG_WARN("EmotesText.dbc: could not derive field offsets for {}", path);
    return false;
  }

  for (uint32_t rec = 0; rec < recordCount; ++rec) {
    uint32_t const id =
        reader.ReadUInt32(rec, 0, fieldOffsets);
    if (id == 0u)
      continue;
    uint32_t const emoteAnim =
        reader.ReadUInt32(rec, 2, fieldOffsets);
    m_textEmoteToAnim.emplace(id, emoteAnim);
  }

  m_loaded = true;
  LOG_DEBUG("EmotesText.dbc: {} text emotes from {}.", m_textEmoteToAnim.size(),
            path);
  return true;
  }

std::optional<uint32_t>
EmotesTextDbc::LookupEmoteAnim(uint32_t textEmoteId) const {
  if (!m_loaded)
    return std::nullopt;
  auto it = m_textEmoteToAnim.find(textEmoteId);
  if (it == m_textEmoteToAnim.end())
    return std::nullopt;
  return it->second;
  }

} // namespace Firelands
