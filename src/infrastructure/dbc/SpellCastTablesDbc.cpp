#include <infrastructure/dbc/SpellCastTablesDbc.h>

#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <algorithm>
#include <string_view>

namespace Firelands {

namespace {

// DBCfmt.h
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

static bool LoadSpellRange(
    std::string const &path,
    std::unordered_map<uint32, SpellCastTablesDbc::SpellRangeRow> &outById) {
  outById.clear();
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
  outById.reserve(static_cast<size_t>(n));
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0, offsets);
    if (id == 0u)
      continue;
    SpellCastTablesDbc::SpellRangeRow row{};
    row.hostileMinYards = std::max(0.f, reader.ReadFloat(rec, 1, offsets));
    row.friendlyMinYards = std::max(0.f, reader.ReadFloat(rec, 2, offsets));
    row.hostileMaxYards = std::max(0.f, reader.ReadFloat(rec, 3, offsets));
    row.friendlyMaxYards = std::max(0.f, reader.ReadFloat(rec, 4, offsets));
    outById.emplace(id, row);
  }
  LOG_DEBUG("SpellRange.dbc: {} rows from {}.", outById.size(), path);
  return true;
}

// SpellPowerEntryfmt[] = "diiiixxf";
static bool LoadSpellPower(std::string const &path,
                            std::unordered_map<uint32, uint32> &outManaById) {
  outManaById.clear();
  if (path.empty())
    return false;
  DbcReader reader;
  if (!reader.Load(path)) {
    LOG_WARN("SpellPower.dbc not found or unreadable: {}", path);
    return false;
  }
  constexpr std::string_view kFmt = "diiiixxf";
  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kFmt);
  if (!reader.VerifyFormat(kFmt)) {
    LOG_WARN("SpellPower.dbc: field count mismatch (path={})", path);
    return false;
  }
  char const last = kFmt[kFmt.size() - 1];
  size_t const expected =
      static_cast<size_t>(offsets.back()) +
      (((last == 'b') || (last == 'X')) ? 1u : 4u);
  if (expected != static_cast<size_t>(reader.GetRecordSize())) {
    LOG_WARN("SpellPower.dbc: record size {} expected {} (path={})",
             reader.GetRecordSize(), expected, path);
    return false;
  }

  uint32_t const n = reader.GetRecordCount();
  outManaById.reserve(static_cast<size_t>(n));
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0, offsets);
    if (id == 0u)
      continue;
    uint32_t const mana = reader.ReadUInt32(rec, 1, offsets);
    outManaById.emplace(id, mana);
  }
  LOG_DEBUG("SpellPower.dbc: {} rows from {}.", outManaById.size(), path);
  return true;
}

// SpellCategoriesEntryfmt[] = "diiiiii";
static bool LoadSpellCategories(
    std::string const &path,
    std::unordered_map<uint32, uint32> &outCategoriesRowIdToCategoryGroup) {
  outCategoriesRowIdToCategoryGroup.clear();
  if (path.empty())
    return false;
  constexpr std::string_view kFmt = "diiiiii";
  DbcReader reader;
  if (!reader.Load(path)) {
    LOG_WARN("SpellCategories.dbc not found or unreadable: {}", path);
    return false;
  }
  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kFmt);
  if (!reader.VerifyFormat(kFmt)) {
    LOG_WARN("SpellCategories.dbc: field count mismatch (path={})", path);
    return false;
  }
  char const last = kFmt[kFmt.size() - 1];
  size_t const expected =
      static_cast<size_t>(offsets.back()) +
      (((last == 'b') || (last == 'X')) ? 1u : 4u);
  if (expected != static_cast<size_t>(reader.GetRecordSize())) {
    LOG_WARN("SpellCategories.dbc: record size {} expected {} (path={})",
             reader.GetRecordSize(), expected, path);
    return false;
  }

  uint32_t const n = reader.GetRecordCount();
  outCategoriesRowIdToCategoryGroup.reserve(static_cast<size_t>(n));
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, 0, offsets);
    if (id == 0u)
      continue;
    uint32_t const category = reader.ReadUInt32(rec, 1, offsets);
    outCategoriesRowIdToCategoryGroup.emplace(id, category);
  }
  LOG_DEBUG("SpellCategories.dbc: {} rows from {}.",
            outCategoriesRowIdToCategoryGroup.size(), path);
  return !outCategoriesRowIdToCategoryGroup.empty();
}

} // namespace

bool SpellCastTablesDbc::Load(std::string const &spellCastTimesPath,
                              std::string const &spellRangePath,
                              std::string const &spellCooldownsPath,
                              std::string const &spellPowerPath,
                              std::string const &spellCategoriesPath) {
  bool const ct = LoadCastTimes(spellCastTimesPath, m_castBaseMs);
  bool const rg = LoadSpellRange(spellRangePath, m_spellRangeRows);
  bool cd = false;
  m_cooldowns.clear();
  if (!spellCooldownsPath.empty()) {
    constexpr std::string_view kCooldownFmt = "diii";
    DbcReader cdReader;
    if (cdReader.Load(spellCooldownsPath)) {
      std::vector<uint32_t> const cdOffsets =
          DbcBuildFieldByteOffsets(kCooldownFmt);
      if (cdReader.VerifyFormat(kCooldownFmt)) {
        char const lastC = kCooldownFmt[kCooldownFmt.size() - 1];
        size_t const expectedC =
            static_cast<size_t>(cdOffsets.back()) +
            (((lastC == 'b') || (lastC == 'X')) ? 1u : 4u);
        if (expectedC == static_cast<size_t>(cdReader.GetRecordSize())) {
          uint32_t const ncd = cdReader.GetRecordCount();
          m_cooldowns.reserve(static_cast<size_t>(ncd));
          for (uint32_t rec = 0; rec < ncd; ++rec) {
            uint32_t const id = cdReader.ReadUInt32(rec, 0, cdOffsets);
            if (id == 0u)
              continue;
            CooldownRow row{};
            row.categoryRecoveryMs = cdReader.ReadUInt32(rec, 1, cdOffsets);
            row.recoveryMs = cdReader.ReadUInt32(rec, 2, cdOffsets);
            row.startRecoveryMs = cdReader.ReadUInt32(rec, 3, cdOffsets);
            m_cooldowns.emplace(id, row);
          }
          LOG_DEBUG("SpellCooldowns.dbc: {} rows from {}.", m_cooldowns.size(),
                    spellCooldownsPath);
          cd = !m_cooldowns.empty();
        } else {
          LOG_WARN("SpellCooldowns.dbc: record size {} expected {} (path={})",
                   cdReader.GetRecordSize(), expectedC, spellCooldownsPath);
        }
      } else {
        LOG_WARN("SpellCooldowns.dbc: field count mismatch (path={})",
                 spellCooldownsPath);
      }
    } else {
      LOG_WARN("SpellCooldowns.dbc not found or unreadable: {}", spellCooldownsPath);
    }
  }
  m_spellPowerManaCost.clear();
  bool const sp = LoadSpellPower(spellPowerPath, m_spellPowerManaCost);
  m_spellCategoryGroupByCategoriesRowId.clear();
  bool const sc =
      LoadSpellCategories(spellCategoriesPath, m_spellCategoryGroupByCategoriesRowId);
  return ct || rg || cd || sp || sc;
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

float SpellCastTablesDbc::GetSpellRangeMinYards(uint32 rangeIndex,
                                                bool friendlyTarget) const {
  if (rangeIndex == 0u)
    return 0.0f;
  auto it = m_spellRangeRows.find(rangeIndex);
  if (it == m_spellRangeRows.end())
    return 0.0f;
  SpellRangeRow const &r = it->second;
  return friendlyTarget ? r.friendlyMinYards : r.hostileMinYards;
}

float SpellCastTablesDbc::GetSpellRangeMaxYards(uint32 rangeIndex,
                                                bool friendlyTarget) const {
  if (rangeIndex == 0u)
    return 0.0f;
  auto it = m_spellRangeRows.find(rangeIndex);
  if (it == m_spellRangeRows.end())
    return 0.0f;
  SpellRangeRow const &r = it->second;
  return friendlyTarget ? r.friendlyMaxYards : r.hostileMaxYards;
}

void SpellCastTablesDbc::GetCooldownTiming(uint32 cooldownsId, uint32 *categoryRecoveryMs,
                                           uint32 *recoveryMs, uint32 *startRecoveryMs) const {
  if (categoryRecoveryMs)
    *categoryRecoveryMs = 0;
  if (recoveryMs)
    *recoveryMs = 0;
  if (startRecoveryMs)
    *startRecoveryMs = 0;
  if (cooldownsId == 0u)
    return;
  auto it = m_cooldowns.find(cooldownsId);
  if (it == m_cooldowns.end())
    return;
  if (categoryRecoveryMs)
    *categoryRecoveryMs = it->second.categoryRecoveryMs;
  if (recoveryMs)
    *recoveryMs = it->second.recoveryMs;
  if (startRecoveryMs)
    *startRecoveryMs = it->second.startRecoveryMs;
}

uint32 SpellCastTablesDbc::GetSpellPowerManaCost(uint32 spellPowerId) const {
  if (spellPowerId == 0u)
    return 0u;
  auto it = m_spellPowerManaCost.find(spellPowerId);
  if (it == m_spellPowerManaCost.end())
    return 0u;
  return it->second;
}

uint32 SpellCastTablesDbc::GetSpellCategoryGroupForCategoriesId(
    uint32 categoriesId) const {
  if (categoriesId == 0u)
    return 0u;
  auto it = m_spellCategoryGroupByCategoriesRowId.find(categoriesId);
  if (it == m_spellCategoryGroupByCategoriesRowId.end())
    return 0u;
  return it->second;
}

} // namespace Firelands
