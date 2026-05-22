#include <shared/dbc/StarterSpellsDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/game/QuestMask.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/game/StarterSkillFilters.h>
#include <shared/game/StarterSpellFilters.h>
#include <shared/Logger.h>

#include <algorithm>
#include <tuple>

namespace Firelands {

namespace {

constexpr char kSkillLineAbilityFmt[] = "niiiiiiiiiiiii";
constexpr char kSkillRaceClassInfoFmt[] = "diiiiiiii";
bool MaskAllowsPlayer(uint32_t mask, uint32_t playerMask) {
  if (mask == 0u)
    return true;
  if (mask == 0xFFFFFFFFu)
    return true;
  return (mask & playerMask) != 0u;
}

/// Ref `AbilityLearnType`: 0 trainer, 1 on skill value (Shoot/Throw at rank 1),
/// 2 on skill learn (Attack, etc.). Starter rows use min rank 0–1 only.
bool AllowsStarterSkillLineAcquire(uint32_t acquireMethod) {
  return acquireMethod == 0u || acquireMethod == 1u || acquireMethod == 2u;
}

} // namespace

bool StarterSpellsDbc::Load(std::string const &skillLineAbilityPath,
                            std::string const &skillRaceClassInfoPath) {
  m_loaded = false;
  m_abilities.clear();
  m_skillRaceClass.clear();

  DbcReader srciReader;
  if (!srciReader.Load(skillRaceClassInfoPath)) {
    LOG_WARN("StarterSpellsDbc: failed to load {}", skillRaceClassInfoPath);
    return false;
  }
  std::vector<uint32_t> const srciOffs =
      DbcBuildFieldByteOffsets(kSkillRaceClassInfoFmt);
  if (!srciReader.VerifyFormat(kSkillRaceClassInfoFmt)) {
    LOG_WARN("StarterSpellsDbc: SkillRaceClassInfo.dbc field count mismatch");
    return false;
  }
  for (uint32_t rec = 0; rec < srciReader.GetRecordCount(); ++rec) {
    uint32_t const skillId = srciReader.ReadUInt32(rec, 1, srciOffs);
    if (skillId == 0u)
      continue;
    m_skillRaceClass.emplace_back(
        skillId, srciReader.ReadUInt32(rec, 2, srciOffs),
        srciReader.ReadUInt32(rec, 3, srciOffs));
  }

  DbcReader slaReader;
  if (!slaReader.Load(skillLineAbilityPath)) {
    LOG_WARN("StarterSpellsDbc: failed to load {}", skillLineAbilityPath);
    return false;
  }
  std::vector<uint32_t> const slaOffs =
      DbcBuildFieldByteOffsets(kSkillLineAbilityFmt);
  if (!slaReader.VerifyFormat(kSkillLineAbilityFmt)) {
    LOG_WARN("StarterSpellsDbc: SkillLineAbility.dbc field count mismatch");
    return false;
  }
  m_abilities.reserve(slaReader.GetRecordCount());
  for (uint32_t rec = 0; rec < slaReader.GetRecordCount(); ++rec) {
    SkillLineAbilityRow row;
    row.skillLine = slaReader.ReadUInt32(rec, 1, slaOffs);
    row.spellId = slaReader.ReadUInt32(rec, 2, slaOffs);
    row.raceMask = slaReader.ReadUInt32(rec, 3, slaOffs);
    row.classMask = slaReader.ReadUInt32(rec, 4, slaOffs);
    row.minSkillLineRank = slaReader.ReadUInt32(rec, 7, slaOffs);
    row.supercededBySpell = slaReader.ReadUInt32(rec, 8, slaOffs);
    row.acquireMethod = slaReader.ReadUInt32(rec, 9, slaOffs);
    if (row.spellId == 0u)
      continue;
    m_abilities.push_back(row);
  }

  m_loaded = true;
  LOG_DEBUG("StarterSpellsDbc: {} SkillLineAbility rows, {} race/class skill "
            "links.",
            m_abilities.size(), m_skillRaceClass.size());
  return true;
}

std::vector<uint32_t> StarterSpellsDbc::finalizeCandidates(
    std::unordered_set<uint32_t> candidates) const {
  std::unordered_map<uint32_t, uint32_t> supercededBy;
  for (SkillLineAbilityRow const &row : m_abilities) {
    if (!candidates.count(row.spellId))
      continue;
    if (row.supercededBySpell != 0u)
      supercededBy[row.spellId] = row.supercededBySpell;
  }

  std::unordered_set<uint32_t> removeSuperseded;
  for (uint32_t spellId : candidates) {
    auto it = supercededBy.find(spellId);
    if (it == supercededBy.end())
      continue;
    uint32_t const newer = it->second;
    if (candidates.count(newer))
      removeSuperseded.insert(newer);
  }

  for (uint32_t spellId : removeSuperseded)
    candidates.erase(spellId);

  std::vector<uint32_t> out(candidates.begin(), candidates.end());
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<uint32_t> StarterSpellsDbc::collectSkillLineSpells(
    uint8_t race, uint8_t klass, bool weaponArmorLanguageOnly) const {
  if (!m_loaded || klass == 0u)
    return {};

  uint32_t const raceMask = PlayerRaceMask(race);
  uint32_t const classMask = PlayerClassMask(klass);

  std::unordered_set<uint32_t> skillLines;
  for (auto const &[skillId, srcRaceMask, srcClassMask] : m_skillRaceClass) {
    if (!MaskAllowsPlayer(srcClassMask, classMask))
      continue;
    if (!MaskAllowsPlayer(srcRaceMask, raceMask))
      continue;
    if (IsExcludedSpellGrantSkillLine(skillId))
      continue;
    if (weaponArmorLanguageOnly && !IsAllowedStarterSkillLine(skillId))
      continue;
    skillLines.insert(skillId);
  }

  std::unordered_set<uint32_t> candidates;
  for (SkillLineAbilityRow const &row : m_abilities) {
    // Attack (6603) is on GENERIC/DND (183), which is not a starter skill line,
    // but every new character must know it — ref Player::learnDefaultSpells.
    if (row.skillLine == 183u && row.spellId == 6603u) {
      if (row.minSkillLineRank <= 1u &&
          AllowsStarterSkillLineAcquire(row.acquireMethod) &&
          MaskAllowsPlayer(row.classMask, classMask)) {
        candidates.insert(row.spellId);
      }
      continue;
    }
    if (!skillLines.count(row.skillLine))
      continue;
    if (IsExcludedSpellGrantSkillLine(row.skillLine))
      continue;
    if (row.minSkillLineRank > 1u)
      continue;
    if (!AllowsStarterSkillLineAcquire(row.acquireMethod))
      continue;
    if (!MaskAllowsPlayer(row.classMask, classMask))
      continue;
    if (row.raceMask != 0u && row.raceMask != 0xFFFFFFFFu &&
        !MaskAllowsPlayer(row.raceMask, raceMask))
      continue;
    if (IsRidingOrTransportStarterSpell(row.spellId))
      continue;
    if (IsWarlockQuestGatedSummonSpell(row.spellId))
      continue;
    candidates.insert(row.spellId);
  }

  return finalizeCandidates(std::move(candidates));
}

std::vector<uint32_t> StarterSpellsDbc::GetStarterSpells(uint8_t race,
                                                         uint8_t klass) const {
  return collectSkillLineSpells(race, klass, false);
}

std::vector<uint32_t> StarterSpellsDbc::GetWeaponArmorLanguageStarterSpells(
    uint8_t race, uint8_t klass) const {
  return collectSkillLineSpells(race, klass, true);
}

bool StarterSpellsDbc::IsSpellFromExcludedSkillLine(uint32_t spellId) const {
  if (!m_loaded || spellId == 0u)
    return false;
  for (SkillLineAbilityRow const &row : m_abilities) {
    if (row.spellId != spellId)
      continue;
    return IsExcludedSpellGrantSkillLine(row.skillLine);
  }
  return false;
}

std::vector<uint32_t> StarterSpellsDbc::GetRacialSpells(uint8_t race,
                                                        uint8_t klass) const {
  if (!m_loaded || klass == 0u || race == 0u)
    return {};

  uint32_t const raceMask = PlayerRaceMask(race);
  uint32_t const classMask = PlayerClassMask(klass);

  std::unordered_set<uint32_t> candidates;
  for (SkillLineAbilityRow const &row : m_abilities) {
    if (row.raceMask == 0u || row.raceMask == 0xFFFFFFFFu)
      continue;
    if (!MaskAllowsPlayer(row.raceMask, raceMask))
      continue;
    if (!MaskAllowsPlayer(row.classMask, classMask))
      continue;
    if (row.minSkillLineRank > 1u)
      continue;
    if (!AllowsStarterSkillLineAcquire(row.acquireMethod))
      continue;
    if (IsRidingOrTransportStarterSpell(row.spellId))
      continue;
    candidates.insert(row.spellId);
  }

  return finalizeCandidates(std::move(candidates));
}

} // namespace Firelands
