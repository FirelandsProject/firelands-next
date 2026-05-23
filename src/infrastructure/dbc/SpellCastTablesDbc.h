#pragma once

#include <domain/repositories/ISpellCastTables.h>
#include <shared/game/SpellPowerResolve.h>
#include <string>
#include <unordered_map>

namespace Firelands {

/// Loads `SpellCastTimes.dbc`, `SpellRange.dbc`, optional `SpellCooldowns.dbc`, optional
/// `SpellPower.dbc`, and optional `SpellCategories.dbc` (Cataclysm 4.3.4 / DBCfmt.h).
class SpellCastTablesDbc final : public ISpellCastTables {
public:
  struct CooldownRow {
    uint32 categoryRecoveryMs = 0;
    uint32 recoveryMs = 0;
    uint32 startRecoveryMs = 0;
  };

  /// Parsed row from `SpellRange.dbc` (`SpellRangefmt`); field order — hostile index 0,
  /// friendly index 1 for both min and max.
  struct SpellRangeRow {
    float hostileMinYards = 0.f;
    float hostileMaxYards = 0.f;
    float friendlyMinYards = 0.f;
    float friendlyMaxYards = 0.f;
  };

  /// `SpellDuration.dbc` row (base, per level, max cap).
  struct DurationRow {
    uint32 baseMs = 0;
    uint32 perLevelMs = 0;
    uint32 maxMs = 0;
  };

  /// Each file is optional: missing file logs a warning and that table stays empty.
  bool Load(std::string const &spellCastTimesPath, std::string const &spellRangePath,
            std::string const &spellCooldownsPath, std::string const &spellPowerPath,
            std::string const &spellCategoriesPath,
            std::string const &spellDurationPath = {});

  bool HasCastTimes() const { return !m_castBaseMs.empty(); }
  bool HasRanges() const { return !m_spellRangeRows.empty(); }
  bool HasCooldowns() const { return !m_cooldowns.empty(); }
  bool HasSpellCategories() const { return !m_spellCategoryGroupByCategoriesRowId.empty(); }
    bool HasSpellPowerRows() const { return !m_spellPowerById.empty(); }
    size_t SpellPowerRowCount() const { return m_spellPowerById.size(); }

  uint32 GetCastTimeMs(uint32 castingTimeIndex) const override;
  float GetSpellRangeMinYards(uint32 rangeIndex, bool friendlyTarget) const override;
  float GetSpellRangeMaxYards(uint32 rangeIndex, bool friendlyTarget) const override;
  void GetCooldownTiming(uint32 cooldownsId, uint32 *categoryRecoveryMs,
                         uint32 *recoveryMs, uint32 *startRecoveryMs) const override;

    uint32 ResolveSpellPowerCost(uint32 spellPowerId, uint32 spellPowerType,
                                                              uint8 casterLevel, uint8 spellLevel,
                                                              uint32 power1PoolForPercent) const override;

  uint32 GetSpellCategoryGroupForCategoriesId(uint32 categoriesId) const override;

  uint32 GetDurationMs(uint32 durationIndex, uint8 casterLevel) const override;

private:
  std::unordered_map<uint32, int32> m_castBaseMs;
  std::unordered_map<uint32, SpellRangeRow> m_spellRangeRows;
  std::unordered_map<uint32, CooldownRow> m_cooldowns;
    std::unordered_map<uint32, SpellPowerDbcRow> m_spellPowerById;
  /// `SpellCategories.dbc` row id → `Category` field (shared cooldown group key).
  std::unordered_map<uint32, uint32> m_spellCategoryGroupByCategoriesRowId;
  std::unordered_map<uint32, DurationRow> m_durationRows;
  };

} // namespace Firelands
