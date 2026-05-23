#include <infrastructure/dbc/SpellEntryDbcStore.h>

#include <domain/repositories/ISpellCastTables.h>
#include <shared/dbc/DbcReader.h>
#include <shared/game/SpellAuraTypes.h>
#include <shared/game/SpellPowerCost.h>
#include <shared/game/StarterSpellFilters.h>
#include <shared/game/SpellEffectMagnitude.h>
#include <shared/game/StarterSpellFilters.h>
#include <shared/Logger.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Firelands {

namespace {

// SpellEffectEntryfmt[] = "nifiiiffiiiiiifiifiiiiiiiix"
constexpr std::string_view kSpellEffectFmt =
    "nifiiiffiiiiiifiifiiiiiiiix";

constexpr uint32_t kSpellEffectFieldEffect = 1;
/// `ApplyAuraName` (aura type) for 4.3.4 `SpellEffect.dbc` — field index 3, not `EffectMiscValue`.
constexpr uint32_t kSpellEffectFieldApplyAuraName = 3;
/// `EffectAmplitude` (periodic tick ms) — field index 4.
constexpr uint32_t kSpellEffectFieldAmplitude = 4;
constexpr uint32_t kSpellEffectFieldBasePoints = 5;
/// `EffectRealPointsPerLevel` (float) — field index 6 in 4.3.4 `SpellEffect.dbc`.
constexpr uint32_t kSpellEffectFieldRealPointsPerLevel = 6;
constexpr uint32_t kSpellEffectFieldDieSides = 9;
/// `EffectMiscValue` / `EffectMiscValueB` (4.3.4 `SpellEffect.dbc` layout).
constexpr uint32_t kSpellEffectFieldMiscValue = 16;
constexpr uint32_t kSpellEffectFieldMiscValueB = 17;
constexpr uint32_t kSpellEffectFieldSpellID = 24;
constexpr uint32_t kSpellEffectFieldEffectIndex = 25;

constexpr uint32_t SPELL_EFFECT_SCHOOL_DAMAGE = 2;
constexpr uint32_t SPELL_EFFECT_APPLY_AURA = 6;
constexpr uint32_t SPELL_EFFECT_HEALTH_LEECH = 9;
constexpr uint32_t SPELL_EFFECT_HEAL = 10;
constexpr uint32_t SPELL_EFFECT_ENVIRONMENTAL_DAMAGE = 13;

// DBCfmt.h — must match client `Spell.dbc` for 15595.
constexpr std::string_view kSpellEntryFmt =
    "niiiiiiiiiiiiiiifiiiissxxiixxifiiiiiiixiiiiiiiii";

// Field indices = character index in `kSpellEntryFmt` (SpellEntry order).
constexpr uint32_t kFieldId = 0;
constexpr uint32_t kFieldAttributes = 1;
constexpr uint32_t kFieldAttributesEx = 2;
constexpr uint32_t kFieldAttributesEx2 = 3;
constexpr uint32_t kFieldAttributesEx8 = 9;
constexpr uint32_t kFieldCastingTimeIndex = 12;
constexpr uint32_t kFieldDurationIndex = 13;
constexpr uint32_t kFieldPowerType = 14;
constexpr uint32_t kFieldRangeIndex = 15;
constexpr uint32_t kFieldSchoolMask = 25;
constexpr uint32_t kFieldCategoriesId = 35;
constexpr uint32_t kFieldCooldownsId = 37;
/// SpellEntry::PowerDisplayID — row id in `SpellPower.dbc` (SpellInfo::SpellPowerId).
constexpr uint32_t kFieldSpellPowerId = 42;
/// `SpellEntry::LevelsID` → `SpellLevels.dbc`.
constexpr uint32_t kFieldLevelsId = 41;
/// `Spell.dbc` `SpellVisualID[0]` / `[1]` (Cataclysm 4.3.4 layout).
constexpr uint32_t kFieldSpellVisualId0 = 19;
constexpr uint32_t kFieldSpellVisualId1 = 23;

constexpr std::string_view kSpellLevelsFmt = "diii";
constexpr uint32_t kSpellLevelsFieldSpellLevel = 3;

static size_t LastFieldSizeBytes(char lastFmt) {
  return ((lastFmt == 'b') || (lastFmt == 'X')) ? 1u : 4u;
  }

static bool ExpectedRecordLayout(DbcReader const &reader, std::string_view fmt,
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

static char const* const kSpellDbcMergeQueryFull =
    "SELECT `Id`, `Attributes`, `AttributesEx`, `AttributesEx2`, `CastingTimeIndex`, "
    "`DurationIndex`, "
    "`RangeIndex`, "
    "`SchoolMask`, `PowerType`, `OvAttributes`, `OvCastingTimeIndex`, `OvDurationIndex`, "
    "`OvRangeIndex`, `OvSchoolMask` FROM `firelands_world`.`spell_dbc`";

static char const* const kSpellDbcMergeQueryLegacy =
    "SELECT `Id`, `Attributes`, `AttributesEx`, `AttributesEx2`, `CastingTimeIndex`, "
    "`DurationIndex`, "
    "`RangeIndex`, "
    "`SchoolMask`, `PowerType` FROM `firelands_world`.`spell_dbc`";

static void ApplySpellDbcMergeRow(sql::ResultSet& rs, bool hasOvColumns,
                                  std::unordered_map<uint32, SpellDefinition>& byId) {
  uint32 const id = static_cast<uint32>(rs.getUInt("Id"));
  if (id == 0u)
    return;

  uint32 const attributes = static_cast<uint32>(rs.getUInt("Attributes"));
  uint32 const castingTimeIndex =
      static_cast<uint32>(rs.getUInt("CastingTimeIndex"));
  uint32 const durationIndex = static_cast<uint32>(rs.getUInt("DurationIndex"));
  uint32 const rangeIndex = static_cast<uint32>(rs.getUInt("RangeIndex"));
  uint32 const schoolMask = static_cast<uint32>(rs.getUInt("SchoolMask"));
  bool const hasPowerOverride = !rs.isNull("PowerType");
  uint32 const powerOverride =
      hasPowerOverride ? static_cast<uint32>(rs.getUInt("PowerType")) : 0u;

  auto it = byId.find(id);
  if (it != byId.end()) {
    SpellDefinition& d = it->second;
    if (hasPowerOverride)
      d.powerType = powerOverride;
    if (hasOvColumns) {
      if (!rs.isNull("OvAttributes"))
        d.attributes = static_cast<uint32>(rs.getUInt("OvAttributes"));
      if (!rs.isNull("OvCastingTimeIndex"))
        d.castingTimeIndex = static_cast<uint32>(rs.getUInt("OvCastingTimeIndex"));
      if (!rs.isNull("OvDurationIndex"))
        d.durationIndex = static_cast<uint32>(rs.getUInt("OvDurationIndex"));
      if (!rs.isNull("OvRangeIndex"))
        d.rangeIndex = static_cast<uint32>(rs.getUInt("OvRangeIndex"));
      if (!rs.isNull("OvSchoolMask"))
        d.schoolMask = static_cast<uint32>(rs.getUInt("OvSchoolMask"));
  }
    return;
  }

  uint32 const attributesEx = static_cast<uint32>(rs.getUInt("AttributesEx"));
  uint32 const attributesEx2 = static_cast<uint32>(rs.getUInt("AttributesEx2"));
  SpellDefinition d{};
  d.id = id;
  d.attributes = attributes;
  d.attributesEx = attributesEx;
  d.attributesEx2 = attributesEx2;
  d.castingTimeIndex = castingTimeIndex;
  d.durationIndex = durationIndex;
  d.rangeIndex = rangeIndex;
  d.schoolMask = schoolMask;
  d.powerType = hasPowerOverride ? powerOverride : 0u;
    if (hasOvColumns) {
      if (!rs.isNull("OvAttributes"))
        d.attributes = static_cast<uint32>(rs.getUInt("OvAttributes"));
      if (!rs.isNull("OvCastingTimeIndex"))
        d.castingTimeIndex = static_cast<uint32>(rs.getUInt("OvCastingTimeIndex"));
      if (!rs.isNull("OvDurationIndex"))
        d.durationIndex = static_cast<uint32>(rs.getUInt("OvDurationIndex"));
      if (!rs.isNull("OvRangeIndex"))
        d.rangeIndex = static_cast<uint32>(rs.getUInt("OvRangeIndex"));
      if (!rs.isNull("OvSchoolMask"))
        d.schoolMask = static_cast<uint32>(rs.getUInt("OvSchoolMask"));
  }
  byId.emplace(id, d);
  }

} // namespace

bool SpellEntryDbcStore::Load(std::string const &path) {
  m_loaded = false;
  m_byId.clear();

  DbcReader reader;
  if (!reader.Load(path))
    return false;

  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kSpellEntryFmt);
  if (!reader.VerifyFormat(kSpellEntryFmt)) {
    LOG_WARN("Spell.dbc: field count {} does not match SpellEntryfmt length {}",
             reader.GetFieldCount(), kSpellEntryFmt.size());
    return false;
  }
  if (!ExpectedRecordLayout(reader, kSpellEntryFmt, offsets)) {
    LOG_WARN(
        "Spell.dbc: record size {} does not match SpellEntryfmt-derived size (path={})",
        reader.GetRecordSize(), path);
    return false;
  }

  uint32_t const n = reader.GetRecordCount();
  m_byId.reserve(static_cast<size_t>(n));
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const id = reader.ReadUInt32(rec, kFieldId, offsets);
  if (id == 0u)
      continue;
    SpellDefinition def;
    def.id = id;
    def.attributes = reader.ReadUInt32(rec, kFieldAttributes, offsets);
    def.attributesEx = reader.ReadUInt32(rec, kFieldAttributesEx, offsets);
    def.attributesEx2 = reader.ReadUInt32(rec, kFieldAttributesEx2, offsets);
    def.attributesEx8 = reader.ReadUInt32(rec, kFieldAttributesEx8, offsets);
    def.castingTimeIndex = reader.ReadUInt32(rec, kFieldCastingTimeIndex, offsets);
    def.durationIndex = reader.ReadUInt32(rec, kFieldDurationIndex, offsets);
        uint32 const field14 = reader.ReadUInt32(rec, kFieldPowerType, offsets);
        uint32 const field42 = reader.ReadUInt32(rec, kFieldSpellPowerId, offsets);
        AssignSpellPowerFieldsFromDbc(field14, field42, def.powerType, def.spellPowerId);
    def.rangeIndex = reader.ReadUInt32(rec, kFieldRangeIndex, offsets);
    def.schoolMask = reader.ReadUInt32(rec, kFieldSchoolMask, offsets);
    def.categoriesId = reader.ReadUInt32(rec, kFieldCategoriesId, offsets);
    def.cooldownsId = reader.ReadUInt32(rec, kFieldCooldownsId, offsets);
    def.levelsId = reader.ReadUInt32(rec, kFieldLevelsId, offsets);
    def.spellVisualId0 = reader.ReadUInt32(rec, kFieldSpellVisualId0, offsets);
    def.spellVisualId1 = reader.ReadUInt32(rec, kFieldSpellVisualId1, offsets);
    m_byId.emplace(id, def);
  }

  m_loaded = true;
  LOG_DEBUG("Spell.dbc: {} spell definitions from {}.", m_byId.size(), path);
  return true;
  }

void SpellEntryDbcStore::ApplySpellLevelsToDefinitions() {
  for (auto &kv : m_byId) {
    SpellDefinition &d = kv.second;
    if (d.levelsId == 0u)
      continue;
    auto it = m_requiredLevelByLevelsId.find(d.levelsId);
    if (it != m_requiredLevelByLevelsId.end())
      d.requiredLevel = it->second;
  }
  }

bool SpellEntryDbcStore::LoadSpellLevels(std::string const &path) {
  m_requiredLevelByLevelsId.clear();
  DbcReader reader;
  if (!reader.Load(path))
    return false;
  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kSpellLevelsFmt);
  if (!reader.VerifyFormat(kSpellLevelsFmt)) {
    LOG_WARN("SpellLevels.dbc: field count mismatch (path={})", path);
    return false;
  }
  uint32_t const n = reader.GetRecordCount();
  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const levelsId = reader.ReadUInt32(rec, 0, offsets);
    if (levelsId == 0u)
      continue;
    uint32_t const spellLevel =
        reader.ReadUInt32(rec, kSpellLevelsFieldSpellLevel, offsets);
    uint8_t const lvl =
        spellLevel > 255u ? 255u : static_cast<uint8_t>(spellLevel);
    m_requiredLevelByLevelsId[levelsId] = lvl;
  }
  ApplySpellLevelsToDefinitions();
  LOG_DEBUG("SpellLevels.dbc: {} rows from {}.", m_requiredLevelByLevelsId.size(),
            path);
  return true;
  }

void SpellEntryDbcStore::ApplySpellPowerManaFromTables(ISpellCastTables const &tables) {
    (void)tables;
    for (auto &kv : m_byId)
        kv.second.manaCost = 0u;
  }

void SpellEntryDbcStore::MergeSpellDbcRows(std::shared_ptr<sql::Connection> worldConn) {
  if (!worldConn)
    return;
  try {
    std::unique_ptr<sql::Statement> st(worldConn->createStatement());
    std::unique_ptr<sql::ResultSet> rs;
    bool hasOvColumns = true;
  try {
      rs.reset(st->executeQuery(kSpellDbcMergeQueryFull));
    } catch (sql::SQLException& e) {
      if (e.getErrorCode() != 1054) {
        throw;
  }
      hasOvColumns = false;
      LOG_DEBUG(
          "spell_dbc: Ov* columns missing (apply migration 18); merge uses PowerType "
          "and full-row custom spells only.");
      rs.reset(st->executeQuery(kSpellDbcMergeQueryLegacy));
  }

    size_t merged = 0;
    while (rs->next()) {
      ApplySpellDbcMergeRow(*rs, hasOvColumns, m_byId);
      ++merged;
  }
    if (merged > 0u)
      LOG_INFO("Merged {} `spell_dbc` row(s) over Spell.dbc (world DB).", merged);
    } catch (sql::SQLException& e) {
    if (e.getErrorCode() == 1146) {
    LOG_WARN(
          "`firelands_world.spell_dbc` missing (apply world migrations); using DBC "
          "only.");
    } else {
      LOG_WARN("spell_dbc merge skipped: {}", e.what());
  }
  }
  }

void SpellEntryDbcStore::MergeImmediateHealthFromSpellEffect(
    std::string const &path) {
  if (m_byId.empty())
    return;

  DbcReader reader;
  if (!reader.Load(path)) {
    LOG_WARN("SpellEffect.dbc: failed to load {}.", path);
    return;
  }

  std::vector<uint32_t> const offsets = DbcBuildFieldByteOffsets(kSpellEffectFmt);
  if (!reader.VerifyFormat(kSpellEffectFmt)) {
    LOG_WARN("SpellEffect.dbc: field count {} does not match format length {} (path={}).",
             reader.GetFieldCount(), kSpellEffectFmt.size(), path);
    return;
  }
  if (!ExpectedRecordLayout(reader, kSpellEffectFmt, offsets)) {
    LOG_WARN(
        "SpellEffect.dbc: record size {} does not match format-derived size (path={}).",
        reader.GetRecordSize(), path);
    return;
  }

  struct CandidateRow {
    uint32_t spellId;
    uint32_t effectIndex;
    uint32_t effect;
    int32_t basePoints;
    int32_t dieSides;
  };
  std::vector<CandidateRow> candidates;
  std::unordered_map<uint32_t, std::pair<bool, bool>> polarityBySpell;
  uint32_t const n = reader.GetRecordCount();
  candidates.reserve(static_cast<size_t>(n) / 8u + 8u);

  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const spellId =
        reader.ReadUInt32(rec, kSpellEffectFieldSpellID, offsets);
    if (spellId == 0u || m_byId.find(spellId) == m_byId.end())
      continue;
    uint32_t const effect =
        reader.ReadUInt32(rec, kSpellEffectFieldEffect, offsets);

    if (effect == SPELL_EFFECT_HEAL)
      polarityBySpell[spellId].first = true;
    if (effect == SPELL_EFFECT_SCHOOL_DAMAGE ||
        effect == SPELL_EFFECT_HEALTH_LEECH ||
        effect == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE)
      polarityBySpell[spellId].second = true;
    if (effect == kSpellEffectSkill) {
    auto it = m_byId.find(spellId);
      if (it != m_byId.end())
        it->second.grantsSkillLine = true;
  }

    if (effect != SPELL_EFFECT_SCHOOL_DAMAGE && effect != SPELL_EFFECT_HEAL)
      continue;
    int32_t const basePoints =
        reader.ReadInt32(rec, kSpellEffectFieldBasePoints, offsets);
    int32_t const dieSides =
        reader.ReadInt32(rec, kSpellEffectFieldDieSides, offsets);
    uint32_t const effectIndex =
        reader.ReadUInt32(rec, kSpellEffectFieldEffectIndex, offsets);
    candidates.push_back(CandidateRow{spellId, effectIndex, effect, basePoints,
                                      dieSides});
  }

  for (auto const &[spellId, healHarm] : polarityBySpell) {
    auto it = m_byId.find(spellId);
    if (it == m_byId.end())
      continue;
    it->second.spellEffectHasHealKind = healHarm.first;
    it->second.spellEffectHasHarmKind = healHarm.second;
  }

  std::sort(candidates.begin(), candidates.end(),
            [](CandidateRow const &a, CandidateRow const &b) {
              if (a.spellId != b.spellId)
                return a.spellId < b.spellId;
              return a.effectIndex < b.effectIndex;
            });

  size_t applied = 0;
  uint32_t skipSpellId = 0;
  for (CandidateRow const &row : candidates) {
    if (row.spellId == skipSpellId)
      continue;
    auto it = m_byId.find(row.spellId);
    if (it == m_byId.end()) {
      skipSpellId = row.spellId;
      continue;
  }
      skipSpellId = row.spellId;
    int32_t const delta = SpellEffectMagnitude::SignedImmediateHealthDelta(
        row.effect, row.basePoints, row.dieSides);
    if (delta != 0) {
      it->second.immediateHealthEffectDelta = delta;
      ++applied;
  }
  }

  if (applied > 0u)
      LOG_DEBUG(
        "SpellEffect.dbc: set immediateHealthEffectDelta on {} spell(s) from {}.",
        applied, path);

  struct AuraCandidateRow {
    uint32_t spellId;
    uint32_t effectIndex;
    uint32_t auraType;
    uint32_t periodMs;
    int32_t basePoints;
    int32_t dieSides;
    float realPointsPerLevel = 0.f;
    int32_t miscValue = 0;
    uint32_t miscValueB = 0;
  };
  std::vector<AuraCandidateRow> auraCandidates;
  auraCandidates.reserve(static_cast<size_t>(n) / 8u + 8u);

  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const spellId =
        reader.ReadUInt32(rec, kSpellEffectFieldSpellID, offsets);
    if (spellId == 0u || m_byId.find(spellId) == m_byId.end())
      continue;
    uint32_t const effect =
        reader.ReadUInt32(rec, kSpellEffectFieldEffect, offsets);
    if (effect != SPELL_EFFECT_APPLY_AURA)
      continue;
    auraCandidates.push_back(AuraCandidateRow{
        spellId,
        reader.ReadUInt32(rec, kSpellEffectFieldEffectIndex, offsets),
        reader.ReadUInt32(rec, kSpellEffectFieldApplyAuraName, offsets),
        reader.ReadUInt32(rec, kSpellEffectFieldAmplitude, offsets),
        reader.ReadInt32(rec, kSpellEffectFieldBasePoints, offsets),
        reader.ReadInt32(rec, kSpellEffectFieldDieSides, offsets),
        reader.ReadFloat(rec, kSpellEffectFieldRealPointsPerLevel, offsets),
        reader.ReadInt32(rec, kSpellEffectFieldMiscValue, offsets),
        reader.ReadUInt32(rec, kSpellEffectFieldMiscValueB, offsets)});
  }

  for (uint32_t rec = 0; rec < n; ++rec) {
    uint32_t const spellId =
        reader.ReadUInt32(rec, kSpellEffectFieldSpellID, offsets);
    if (spellId == 0u || m_byId.find(spellId) == m_byId.end())
      continue;
    uint32_t const effect =
        reader.ReadUInt32(rec, kSpellEffectFieldEffect, offsets);
    if (effect == SPELL_EFFECT_APPLY_AURA)
      continue;
    auto it = m_byId.find(spellId);
    if (it == m_byId.end())
      continue;
    SpellEffectRow row{};
    row.effectIndex =
        static_cast<uint8>(std::min<uint32_t>(
            reader.ReadUInt32(rec, kSpellEffectFieldEffectIndex, offsets), 2u));
    row.effectKind = effect;
    row.basePoints = reader.ReadInt32(rec, kSpellEffectFieldBasePoints, offsets);
    row.dieSides = reader.ReadInt32(rec, kSpellEffectFieldDieSides, offsets);
    row.realPointsPerLevel =
        reader.ReadFloat(rec, kSpellEffectFieldRealPointsPerLevel, offsets);
    row.miscValue = reader.ReadInt32(rec, kSpellEffectFieldMiscValue, offsets);
    row.miscValueB = reader.ReadUInt32(rec, kSpellEffectFieldMiscValueB, offsets);
    it->second.effectRows.push_back(row);
  }

  struct AuraRowScore {
    int priority = 0;
    int32 periodicTick = 0;
  };

  auto scoreAuraRow = [](AuraCandidateRow const &row) -> AuraRowScore {
    AuraRowScore s;
    if (row.auraType == kSpellAuraPeriodicHeal) {
      s.periodicTick =
          SpellEffectMagnitude::PeriodicHealTick(row.basePoints, row.dieSides);
      s.priority = 50;
    } else if (row.auraType == kSpellAuraPeriodicDamage) {
      s.periodicTick =
          SpellEffectMagnitude::PeriodicDamageTick(row.basePoints, row.dieSides);
      s.priority = 40;
  }
    if (s.periodicTick != 0)
      s.priority += 100;
    if (row.periodMs > 0u)
      s.priority += 10;
    return s;
  };

  auto betterAuraRow = [&](AuraCandidateRow const &candidate,
                           AuraCandidateRow const &current) {
    AuraRowScore const cand = scoreAuraRow(candidate);
    AuraRowScore const cur = scoreAuraRow(current);
    if (cand.priority != cur.priority)
      return cand.priority > cur.priority;
    int32 const candMag = cand.periodicTick >= 0 ? cand.periodicTick : -cand.periodicTick;
    int32 const curMag = cur.periodicTick >= 0 ? cur.periodicTick : -cur.periodicTick;
    if (candMag != curMag)
      return candMag > curMag;
    return candidate.effectIndex < current.effectIndex;
  };

  std::unordered_map<uint32_t, uint8> activeEffectMaskBySpell;
  for (AuraCandidateRow const &row : auraCandidates) {
    if (row.effectIndex < 3u)
      activeEffectMaskBySpell[row.spellId] |= static_cast<uint8>(1u << row.effectIndex);
    if (IsMountOrVehicleAuraType(row.auraType)) {
    auto it = m_byId.find(row.spellId);
      if (it != m_byId.end())
        it->second.hasMountOrVehicleAura = true;
  }
  }

  std::unordered_map<uint32_t, AuraCandidateRow> bestAuraBySpell;
  for (AuraCandidateRow const &row : auraCandidates) {
    if (m_byId.find(row.spellId) == m_byId.end())
      continue;
    auto it = bestAuraBySpell.find(row.spellId);
    if (it == bestAuraBySpell.end() || betterAuraRow(row, it->second))
      bestAuraBySpell[row.spellId] = row;
  }

  std::unordered_map<uint32_t, std::vector<AuraCandidateRow>> allAurasBySpell;
  for (AuraCandidateRow const &row : auraCandidates) {
    if (m_byId.find(row.spellId) == m_byId.end())
      continue;
    allAurasBySpell[row.spellId].push_back(row);
  }

  uint32_t auraCount = 0;
  for (auto &[spellId, rows] : allAurasBySpell) {
    auto it = m_byId.find(spellId);
    if (it == m_byId.end())
      continue;
    std::sort(rows.begin(), rows.end(),
              [](AuraCandidateRow const &a, AuraCandidateRow const &b) {
              return a.effectIndex < b.effectIndex;
            });
    for (AuraCandidateRow const &row : rows) {
      if (IsExcludedLoginAuraType(row.auraType))
      continue;
      SpellAuraEffectRow ar{};
      ar.effectIndex =
          static_cast<uint8>(std::min<uint32_t>(row.effectIndex, 2u));
      ar.auraType = row.auraType;
      ar.basePoints = row.basePoints;
      ar.dieSides = row.dieSides;
      ar.realPointsPerLevel = row.realPointsPerLevel;
      ar.periodMs = row.periodMs;
      ar.miscValue = row.miscValue;
      ar.miscValueB = row.miscValueB;
      if (row.auraType == kSpellAuraPeriodicDamage) {
        ar.periodicHealthDeltaPerTick =
          SpellEffectMagnitude::PeriodicDamageTick(row.basePoints, row.dieSides);
      } else if (row.auraType == kSpellAuraPeriodicHeal) {
        ar.periodicHealthDeltaPerTick =
          SpellEffectMagnitude::PeriodicHealTick(row.basePoints, row.dieSides);
  }
      it->second.auraEffects.push_back(ar);
  }
    if (it->second.auraEffects.empty())
      continue;

    AuraCandidateRow const *primary = nullptr;
    if (auto bestIt = bestAuraBySpell.find(spellId); bestIt != bestAuraBySpell.end())
      primary = &bestIt->second;
    if (!primary)
      primary = &rows.front();

    it->second.hasAuraEffect = true;
    it->second.auraEffectType = primary->auraType;
    it->second.auraEffectIndex =
        static_cast<uint8>(std::min<uint32_t>(primary->effectIndex, 2u));
    if (auto maskIt = activeEffectMaskBySpell.find(spellId);
        maskIt != activeEffectMaskBySpell.end())
      it->second.auraActiveEffectMask = maskIt->second;
    else
      it->second.auraActiveEffectMask =
          static_cast<uint8>(1u << it->second.auraEffectIndex);
    it->second.auraBasePoints = primary->basePoints;
    it->second.auraDieSides = primary->dieSides;
    it->second.auraRealPointsPerLevel = primary->realPointsPerLevel;
    it->second.auraDurationIndex = it->second.durationIndex;
    if (primary->periodMs > 0u)
      it->second.auraPeriodicPeriodMs = primary->periodMs;
    if (primary->auraType == kSpellAuraPeriodicDamage) {
      it->second.auraPeriodicHealthDeltaPerTick =
          SpellEffectMagnitude::PeriodicDamageTick(primary->basePoints,
                                                   primary->dieSides);
    } else if (primary->auraType == kSpellAuraPeriodicHeal) {
      it->second.auraPeriodicHealthDeltaPerTick =
          SpellEffectMagnitude::PeriodicHealTick(primary->basePoints,
                                                   primary->dieSides);
  }
    ++auraCount;
  }

  if (auraCount > 0u)
    LOG_DEBUG("SpellEffect.dbc: detected auras on {} spell(s) from {}.",
              auraCount, path);
  }

bool SpellEntryDbcStore::HasSpell(uint32 spellId) const {
  if (spellId == 0u)
    return false;
  return m_byId.find(spellId) != m_byId.end();
  }

std::optional<SpellDefinition> SpellEntryDbcStore::GetDefinition(uint32 spellId) const {
  if (spellId == 0u)
    return std::nullopt;
    auto it = m_byId.find(spellId);
    if (it == m_byId.end())
    return std::nullopt;
  return it->second;
  }

} // namespace Firelands
