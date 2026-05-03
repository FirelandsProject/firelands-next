#pragma once

#include <shared/dbc/DbcReader.h>
#include <cstdint>
#include <string>

namespace Firelands {

/// Loads 4.3.4 `gtOCT*.dbc` float tables used for player base HP/MP and per-point scaling.
/// Indexing matches reference `GT_MAX_LEVEL` = 100 rows per class block for class tables.
class GtOctCombatDbc {
public:
  bool Load(std::string const &dbcDirectory);

  bool HasBaseHp() const { return m_baseHpLoaded; }
  bool HasBaseMp() const { return m_baseMpLoaded; }
  bool HasHpPerSta() const { return m_hpPerStaLoaded; }
  bool HasMpPerInt() const { return m_mpPerIntLoaded; }

  float BaseHpByClassLevel(uint8_t klass, uint8_t level) const;
  float BaseMpByClassLevel(uint8_t klass, uint8_t level) const;
  float HpPerStaminaAtPlayerLevel(uint8_t level) const;
  float MpPerIntellectAtPlayerLevel(uint8_t level) const;

private:
  static constexpr uint32_t kGtMaxLevel = 100u;

  static uint32_t ClassLevelIndex(uint8_t klass, uint8_t level);
  static uint32_t LevelIndex(uint8_t level);

  DbcReader m_baseHp;
  DbcReader m_baseMp;
  DbcReader m_hpPerSta;
  DbcReader m_mpPerInt;
  bool m_baseHpLoaded = false;
  bool m_baseMpLoaded = false;
  bool m_hpPerStaLoaded = false;
  bool m_mpPerIntLoaded = false;
};

} // namespace Firelands
