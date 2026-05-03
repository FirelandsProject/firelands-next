#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Firelands {

/// `SpellDifficulty.dbc` (client 4.3.4): SpellDifficultyfmt[] = "niiii"
/// — Id plus four spell ids (10N, 25N, 10H, 25H style difficulty variants).
/// Pairs with world table `spelldifficulty_dbc` for DB-driven overrides.
class SpellDifficultyDbc {
public:
  bool Load(std::string const &path);

  bool IsLoaded() const { return m_loaded; }

  uint32_t GetRowCount() const {
    return static_cast<uint32_t>(m_rows.size());
  }

  /// `index` in [0, 3]. Returns 0 if not loaded, unknown id, or empty slot.
  uint32_t GetSpellId(uint32_t difficultyId, unsigned index) const;

private:
  bool m_loaded = false;
  std::unordered_map<uint32_t, std::array<uint32_t, 4>> m_rows;
};

} // namespace Firelands
