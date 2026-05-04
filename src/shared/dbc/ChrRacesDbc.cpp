#include <shared/dbc/ChrRacesDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <array>
#include <string_view>
#include <vector>

namespace Firelands {

namespace {

// TrinityCore 3.3.5 `ChrRacesEntryfmt[]`.
constexpr std::string_view kChrRacesFmtTrinity335 =
    "niixiixiiixxiissssssssssssssssxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxi";

// Cataclysm 4.3.4 / AzerothCore (fmt[8]/fmt[9] are `x` instead of `i`).
constexpr std::string_view kChrRacesFmtCataclysm =
    "niixiixixxxxiissssssssssssssssxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxi";

static constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
    kChrRacesFmtCandidates = {{
        {kChrRacesFmtCataclysm, "4.x / Cataclysm"},
        {kChrRacesFmtTrinity335, "Trinity 3.3.5"},
    }};

static size_t LastFieldSizeBytes(char lastFmt) {
  return ((lastFmt == 'b') || (lastFmt == 'X')) ? 1u : 4u;
}

static bool RecordLayoutMatches(DbcReader const &reader, std::string_view fmt,
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

static bool LoadFromFmt(DbcReader const &reader, std::string_view fmt,
                        std::unordered_map<uint32_t, uint32_t> &outByRace) {
  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(fmt);
  if (!reader.VerifyFormat(fmt) || !RecordLayoutMatches(reader, fmt, offsets))
    return false;
  outByRace.clear();
  for (uint32_t ri = 0; ri < reader.GetRecordCount(); ++ri) {
    uint32_t const id = reader.ReadUInt32(ri, 0, offsets);
    if (id == 0)
      continue;
    uint32_t const fac = reader.ReadUInt32(ri, 2, offsets);
    if (fac != 0)
      outByRace[id] = fac;
  }
  return !outByRace.empty();
}

/// If the format string drifts, `RaceID` / `FactionID` are still the first two
/// full `uint32` columns after the first `x` in many builds — but Trinity keeps
/// them at byte offsets **0** and **8** for all post-TBC ChrRaces we checked.
static bool LoadFromFirstFieldsHeuristic(
    DbcReader const &reader, std::unordered_map<uint32_t, uint32_t> &outByRace) {
  if (reader.GetRecordSize() < 12u)
    return false;
  outByRace.clear();
  for (uint32_t ri = 0; ri < reader.GetRecordCount(); ++ri) {
    uint32_t const id = reader.ReadUInt32AtRecordByteOffset(ri, 0);
    if (id == 0 || id > 128u)
      continue;
    uint32_t const fac = reader.ReadUInt32AtRecordByteOffset(ri, 8);
    if (fac == 0 || fac > 50000u)
      continue;
    outByRace[id] = fac;
  }
  if (outByRace.empty())
    return false;
  // Minimal sanity: race 1 (Human) must map to template 1 on retail ChrRaces.
  auto h = outByRace.find(1u);
  if (h == outByRace.end() || h->second != 1u) {
    LOG_WARN("ChrRaces.dbc heuristic rejected (race 1 FactionID expected 1).");
    outByRace.clear();
    return false;
  }
  return true;
}

} // namespace

bool ChrRacesDbc::Load(std::string const &path) {
  m_loaded = false;
  m_factionByRaceId.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  for (auto const &[fmt, label] : kChrRacesFmtCandidates) {
    if (LoadFromFmt(reader, fmt, m_factionByRaceId)) {
      m_loaded = true;
      LOG_INFO("ChrRaces.dbc: loaded {} race faction row(s) from {} (layout: {})",
               m_factionByRaceId.size(), path, label);
      return true;
    }
  }

  if (LoadFromFirstFieldsHeuristic(reader, m_factionByRaceId)) {
    m_loaded = true;
    LOG_DEBUG(
        "ChrRaces.dbc: no known layout string matched {}; using byte-offset heuristic "
        "(fields={}, recordSize={}).",
        path, reader.GetFieldCount(), reader.GetRecordSize());
    return true;
  }

  LOG_WARN("ChrRaces.dbc: could not parse {} (wrong client build?)", path);
  return false;
}

std::optional<uint32_t> ChrRacesDbc::FactionTemplateIdForRace(uint8_t race) const {
  if (!m_loaded || race == 0)
    return std::nullopt;
  auto it = m_factionByRaceId.find(static_cast<uint32_t>(race));
  if (it == m_factionByRaceId.end())
    return std::nullopt;
  return it->second;
}

} // namespace Firelands
