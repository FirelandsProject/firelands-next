#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace Firelands {

/// `ChrRaces.dbc` — used for `FactionID` → `FactionTemplate.dbc` on player create/login.
class ChrRacesDbc {
public:
  bool Load(std::string const &path);

  bool IsLoaded() const { return m_loaded; }

  /// `FactionTemplate.dbc` id for `UNIT_FIELD_FACTIONTEMPLATE` (ChrRaces.FactionID).
  std::optional<uint32_t> FactionTemplateIdForRace(uint8_t race) const;

private:
  bool m_loaded = false;
  std::unordered_map<uint32_t, uint32_t> m_factionByRaceId;
};

} // namespace Firelands
