#include <application/services/PlayerCreateInfoService.h>
#include <shared/game/PlayerClass.h>
#include <domain/models/Character.h>
#include <shared/game/PlayerPowerType.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/Logger.h>
#include <shared/game/StarterSpellFilters.h>
#include <shared/game/StarterSkillFilters.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace Firelands {

namespace {

uint32_t SumPrimary(uint16_t base, int16_t raceBonus) {
  int64_t v = static_cast<int64_t>(base) + static_cast<int64_t>(raceBonus);
  v = std::max<int64_t>(1, v);
  v = std::min<int64_t>(v, static_cast<int64_t>(0xFFFFFFFFu));
  return static_cast<uint32_t>(v);
}

void StripRidingSpells(std::vector<uint32_t> &spells) {
  spells.erase(
      std::remove_if(spells.begin(), spells.end(),
                     [](uint32_t sid) {
                       return IsRidingOrTransportStarterSpell(sid);
                     }),
      spells.end());
}

} // namespace

PlayerCreateInfoService::PlayerCreateInfoService(
    std::shared_ptr<IPlayerCreateInfoRepository> repository,
    std::string charStartOutfitDbcPath, std::string clientGameTablesDbcDir)
    : m_repository(std::move(repository)) {
  if (!charStartOutfitDbcPath.empty())
    m_charStartOutfitDbcLoaded =
        m_charStartOutfitDbc.Load(charStartOutfitDbcPath);
  if (!clientGameTablesDbcDir.empty() && !m_gtOct.Load(clientGameTablesDbcDir)) {
    LOG_DEBUG("PlayerCreateInfoService: optional gtOCT*.dbc not loaded from {}",
              clientGameTablesDbcDir);
  }
  if (!clientGameTablesDbcDir.empty()) {
    m_statGameTables.Load(clientGameTablesDbcDir);
  }
}

std::vector<uint32_t> PlayerCreateInfoService::GetStarterSpells(uint8_t race,
                                                              uint8_t klass) const {
  std::vector<uint32_t> spells;
  if (m_repository)
    spells = m_repository->GetStarterSpells(race, klass);
  StripRidingSpells(spells);

  if (spells.empty()) {
    LOG_WARN(
        "No starter spells for race={} class={} (apply world migrations "
        "43/45/62_world_playercreateinfo*.sql).",
        static_cast<uint32_t>(race), static_cast<uint32_t>(klass));
  }
  return spells;
}

std::vector<uint32_t> PlayerCreateInfoService::GetRacialSpells(uint8_t race,
                                                              uint8_t klass) const {
  if (!m_repository)
    return {};
  return m_repository->GetRacialStarterSpells(race, klass);
}

std::vector<StarterSkillGrant> PlayerCreateInfoService::GetStarterSkills(
    uint8_t race, uint8_t klass) const {
  if (!m_repository)
    return {};
  std::vector<StarterSkillGrant> skills =
      m_repository->GetStarterSkills(race, klass);
  skills.erase(
      std::remove_if(skills.begin(), skills.end(),
                     [](StarterSkillGrant const &g) {
                       return IsExcludedStarterSkill(g.skillId);
                     }),
      skills.end());
  return skills;
}

uint32_t PlayerCreateInfoService::GetXpToNextLevelForLevel(uint8_t level) const {
  constexpr uint8_t kMaxLevel = 85;
  if (level == 0 || level >= kMaxLevel)
    return 0;
  if (m_repository) {
    uint32_t const v = m_repository->GetXpForNextLevel(level);
    if (v != 0u)
      return v;
  }
  return 400u;
}

bool PlayerCreateInfoService::TryApplyTemplateCombatState(Character &character) {
  if (!m_repository)
    return false;

  uint8_t const klass = ToClassId(character.GetClass());
  uint8_t const level = character.GetLevel();
  uint8_t const race = character.GetRace();

  std::optional<PlayerClassLevelStats> clsRow =
      m_repository->GetClassLevelStats(klass, level);
  if (!clsRow) {
    auto const fallback = Character::GetDefaultPrimaryStats(klass);
    clsRow = PlayerClassLevelStats{static_cast<uint16_t>(fallback[0]),
                                   static_cast<uint16_t>(fallback[1]),
                                   static_cast<uint16_t>(fallback[2]),
                                   static_cast<uint16_t>(fallback[3]),
                                   static_cast<uint16_t>(fallback[4])};
    LOG_DEBUG(
        "TryApplyTemplateCombatState: no player_classlevelstats row for "
        "class={} level={} (guid={}); using built-in fallback (apply "
        "sql/17_player_class_and_race_stats.sql or sql/z_ensure_player_classlevelstats_seed.sql on "
        "firelands_world).",
        static_cast<unsigned>(klass), static_cast<unsigned>(level),
        character.GetGuid());
  }

  PlayerRaceStats raceBonus{};
  if (auto r = m_repository->GetRaceStats(race))
    raceBonus = *r;

  std::array<uint32_t, 5> const prim{
      SumPrimary(clsRow->str, raceBonus.str), SumPrimary(clsRow->agi, raceBonus.agi),
      SumPrimary(clsRow->sta, raceBonus.sta), SumPrimary(clsRow->inte, raceBonus.inte),
      SumPrimary(clsRow->spi, raceBonus.spi)};
  uint32_t const sta = prim[2];
  uint32_t const inte = prim[3];

  PlayerPowerType const pt = GetDefaultPlayerPowerType(klass);
  uint8_t const powerByte = static_cast<uint8_t>(pt);

  double baseHp = static_cast<double>(m_gtOct.BaseHpByClassLevel(klass, level));
  float const hpPerSta = m_gtOct.HpPerStaminaAtPlayerLevel(level);
  uint32_t maxHp = 0;
  if (baseHp > 0.0 && hpPerSta > 0.f)
    maxHp = static_cast<uint32_t>(std::floor(baseHp + static_cast<double>(sta) *
                                              static_cast<double>(hpPerSta)));
  if (maxHp == 0)
    maxHp = 15u + static_cast<uint32_t>(level) * 6u + sta * 8u;
  maxHp = std::max(1u, maxHp);

  uint32_t maxPow1 = DefaultMaxPower1(pt);
  uint32_t pow1 = 0;
  if (pt == PlayerPowerType::Mana) {
    double baseMp = static_cast<double>(m_gtOct.BaseMpByClassLevel(klass, level));
    float const mpPerInt = m_gtOct.MpPerIntellectAtPlayerLevel(level);
    if (baseMp > 0.0 && mpPerInt > 0.f)
      maxPow1 = static_cast<uint32_t>(
          std::floor(baseMp + static_cast<double>(inte) * static_cast<double>(mpPerInt)));
    if (maxPow1 == 0)
      maxPow1 = 50u + inte * 15u;
    maxPow1 = std::max(1u, maxPow1);
    pow1 = maxPow1;
  } else if (pt == PlayerPowerType::Energy || pt == PlayerPowerType::Focus) {
    pow1 = maxPow1;
  } else {
    pow1 = 0;
  }

  character.ApplyCombatStateFromTemplate(prim, maxHp, maxHp, pow1, maxPow1,
                                         powerByte);
  return true;
}

} // namespace Firelands
