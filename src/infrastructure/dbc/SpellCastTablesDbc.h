#pragma once

#include <domain/repositories/ISpellCastTables.h>
#include <string>
#include <unordered_map>

namespace Firelands {

/// Loads `SpellCastTimes.dbc` and `SpellRange.dbc` (Cataclysm 4.3.4 / TCPP `DBCfmt.h`).
class SpellCastTablesDbc final : public ISpellCastTables {
public:
  /// Each file is optional: missing file logs a warning and that table stays empty.
  bool Load(std::string const &spellCastTimesPath,
            std::string const &spellRangePath);

  bool HasCastTimes() const { return !m_castBaseMs.empty(); }
  bool HasRanges() const { return !m_rangeMaxYards.empty(); }

  uint32 GetCastTimeMs(uint32 castingTimeIndex) const override;
  float GetHostileRangeMaxYards(uint32 rangeIndex) const override;

private:
  std::unordered_map<uint32, int32> m_castBaseMs;
  std::unordered_map<uint32, float> m_rangeMaxYards;
};

} // namespace Firelands
