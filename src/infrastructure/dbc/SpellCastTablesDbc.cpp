#include <infrastructure/dbc/SpellCastTablesDbc.h>

#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace Firelands {

namespace {

// TCPP `DBCfmt.h`
constexpr std::string_view kSpellCastTimeFmt = "nixx";
constexpr std::string_view kSpellRangeFmt = "nffffixx";

static bool LoadCastTimes(std::string const &path,
                          std::unordered_map<uint32, int32> &outById) {
  outById.clear();
  DbcReader reader;
  if (!reader.Load(path)) {
    LOG_WARN("SpellCastTimes.dbc not found or unreadable: {}", path);
    return false;
  }
  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kSpellCastTimeFmt);
  if (!reader.VerifyFormat(kSpellCastTimeFmt)) {
    LOG_WARN("SpellCastTimes.dbc: field count mismatch (path={})", path);
    return false;
  }
  char const last = kSpellCastTimeFmt[kSpellCastTimeFmt.size() - 1];
  size_t const expected =
      static_cast<size_t>(offsets.back()) +
      (((last == 'b') || (last == 'X')) ? 1u : 4u);
  if (expected != static_cast<size_t>(reader.GetRecordSize())) {
    LOG_WARN("SpellCastTimes.dbc: record size {} expected {} (path={})",
             reader.GetRecordSize(), expected, path);
    return false;
  }

  uint32_t const n = reader.GetRecordCount();
  outById.reserve(static_cast<size_t>(n));
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0, offsets);
    if (id == 0u)
      continue;
    int32_t const base = reader.ReadInt32(rec, 1, offsets);
    outById.emplace(id, base);
  }
  LOG_DEBUG("SpellCastTimes.dbc: {} rows from {}.", outById.size(), path);
  return true;
}

static bool LoadSpellRange(std::string const &path,
                           std::unordered_map<uint32, float> &outMaxYards) {
  outMaxYards.clear();
  DbcReader reader;
  if (!reader.Load(path)) {
    LOG_WARN("SpellRange.dbc not found or unreadable: {}", path);
    return false;
  }
  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kSpellRangeFmt);
  if (!reader.VerifyFormat(kSpellRangeFmt)) {
    LOG_WARN("SpellRange.dbc: field count mismatch (path={})", path);
    return false;
  }
  char const last = kSpellRangeFmt[kSpellRangeFmt.size() - 1];
  size_t const expected =
      static_cast<size_t>(offsets.back()) +
      (((last == 'b') || (last == 'X')) ? 1u : 4u);
  if (expected != static_cast<size_t>(reader.GetRecordSize())) {
    LOG_WARN("SpellRange.dbc: record size {} expected {} (path={})",
             reader.GetRecordSize(), expected, path);
    return false;
  }

  uint32_t const n = reader.GetRecordCount();
  outMaxYards.reserve(static_cast<size_t>(n));
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0, offsets);
    if (id == 0u)
      continue;
    float const max0 = reader.ReadFloat(rec, 3, offsets);
    float const max1 = reader.ReadFloat(rec, 4, offsets);
    float const yards = std::max(0.0f, std::max(max0, max1));
    outMaxYards.emplace(id, yards);
  }
  LOG_DEBUG("SpellRange.dbc: {} rows from {}.", outMaxYards.size(), path);
  return true;
}

} // namespace

bool SpellCastTablesDbc::Load(std::string const &spellCastTimesPath,
                              std::string const &spellRangePath) {
  bool const ct = LoadCastTimes(spellCastTimesPath, m_castBaseMs);
  bool const rg = LoadSpellRange(spellRangePath, m_rangeMaxYards);
  return ct || rg;
}

uint32 SpellCastTablesDbc::GetCastTimeMs(uint32 castingTimeIndex) const {
  if (castingTimeIndex == 0u)
    return 0u;
  auto it = m_castBaseMs.find(castingTimeIndex);
  if (it == m_castBaseMs.end())
    return 0u;
  if (it->second <= 0)
    return 0u;
  return static_cast<uint32>(it->second);
}

float SpellCastTablesDbc::GetHostileRangeMaxYards(uint32 rangeIndex) const {
  if (rangeIndex == 0u)
    return 0.0f;
  auto it = m_rangeMaxYards.find(rangeIndex);
  if (it == m_rangeMaxYards.end())
    return 0.0f;
  return it->second;
}

} // namespace Firelands
