#pragma once

#include <shared/dbc/DbcReader.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Firelands {

/// Client 4.3.4 `gtCombatRatings.dbc`, `gtChanceToMeleeCrit.dbc`, `gtChanceToSpellCrit.dbc`,
/// `gtChanceToSpellCritBase.dbc` (WDBC: each record is `uint32 id` + `float value`, 8 bytes).
///
/// `gtCombatRatings` rows are indexed as `combatRatingIndex * 100 + (level - 1)` with
/// `combatRatingIndex` matching `CombatRating` (0..31) and level 1..100 in the file.
///
/// `gtChanceToMeleeCrit` / `gtChanceToSpellCrit` rows: `(classId - 1) * 100 + (level - 1)` for
/// classes 1..11 and level 1..100.
class GtPlayerStatGameTables {
public:
  bool Load(std::string const &dbcDirectory);

  bool HasCombatRatings() const { return m_combatRatingsLoaded; }
  bool HasMeleeCritTable() const { return m_meleeCritLoaded; }
  bool HasSpellCritTables() const {
    return m_spellCritLoaded && m_spellCritBaseLoaded;
  }

  /// Rating points required for 1% effect (divide rating by this value).
  float CombatRatingPerPercent(uint8_t combatRatingIndex, uint8_t level) const;

  float ChanceToMeleeCrit(uint8_t classId, uint8_t level) const;
  float ChanceToSpellCrit(uint8_t classId, uint8_t level) const;
  /// reads `gtChanceToSpellCritBase` row by class (fraction before ×100).
  float ChanceToSpellCritBase(uint8_t classId) const;

private:
  static float ReadIdFloatPair(DbcReader const &reader, uint32_t recordIndex,
                               std::vector<uint32_t> const &offsets);

  DbcReader m_combatRatings;
  DbcReader m_meleeCrit;
  DbcReader m_spellCrit;
  DbcReader m_spellCritBase;
  std::vector<uint32_t> m_offIf;
  bool m_combatRatingsLoaded = false;
  bool m_meleeCritLoaded = false;
  bool m_spellCritLoaded = false;
  bool m_spellCritBaseLoaded = false;
};

} // namespace Firelands
