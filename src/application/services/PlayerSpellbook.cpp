#include <shared/game/PlayerClass.h>
#include <application/services/PlayerSpellbook.h>
#include <application/services/PlayerCreateInfoService.h>
#include <shared/game/ChatLanguages.h>
#include <domain/models/Chat.h>
#include <shared/game/SpellLevelGate.h>
#include <shared/game/SkillLineCategories.h>
#include <shared/game/StarterSpellFilters.h>
#include <shared/game/StarterSkillFilters.h>
#include <shared/Logger.h>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace Firelands {
namespace PlayerSpellbook {

namespace {

void PushUnique(std::vector<uint32_t> &out, uint32_t sid) {
  if (sid == 0u)
    return;
  if (std::find(out.begin(), out.end(), sid) == out.end())
    out.push_back(sid);
}

bool IsProfessionGrantSpell(ISpellDefinitionStore const *store, uint32_t spellId) {
  if (IsLanguagePassiveSpell(spellId))
    return false;
  if (!store)
    return false;
  std::optional<SpellDefinition> def = store->GetDefinition(spellId);
  return def && def->grantsSkillLine;
}

bool IsMountOrVehicleSpell(ISpellDefinitionStore const *store, uint32_t spellId) {
  if (IsLanguagePassiveSpell(spellId))
    return false;
  if (IsRidingOrTransportStarterSpell(spellId) || IsKnownMountSpell(spellId))
    return true;
  if (!store)
    return false;
  std::optional<SpellDefinition> def = store->GetDefinition(spellId);
  return def && def->hasMountOrVehicleAura;
}

bool IsExcludedKnownSpell(ISpellDefinitionStore const *store, uint32_t spellId) {
  return IsGuildPerkSpell(spellId) || IsWarlockQuestGatedSummonSpell(spellId) ||
         IsMountOrVehicleSpell(store, spellId) ||
         IsProfessionGrantSpell(store, spellId);
}

bool SpellAllowedAtLevel(ISpellDefinitionStore const *store, uint32_t spellId,
                         uint8_t level) {
  if (!store)
    return true;
  auto def = store->GetDefinition(spellId);
  if (!def)
    return true;
  return SpellMeetsCasterLevelRequirement(def->requiredLevel, level);
}

void StripMountAndRidingSpells(ISpellDefinitionStore const *store,
                               std::vector<uint32_t> &spells) {
  spells.erase(std::remove_if(spells.begin(), spells.end(),
                              [&](uint32_t sid) {
                                return IsMountOrVehicleSpell(store, sid);
                              }),
               spells.end());
}

void FilterBySpellDbc(ISpellDefinitionStore const *store, uint8_t race,
                      uint8_t klass, std::vector<uint32_t> &spells) {
  if (!store)
    return;
  for (auto it = spells.begin(); it != spells.end();) {
    uint32_t const sid = *it;
    if (!IsLanguagePassiveSpell(sid) && !store->HasSpell(sid)) {
      LOG_WARN(
          "Spell id {} not in Spell.dbc; omitted from known spells (race={} "
          "class={})",
          sid, static_cast<uint32_t>(race), static_cast<uint32_t>(klass));
      it = spells.erase(it);
    } else
      ++it;
  }
}

bool IsRacialLanguageSkillId(uint32_t skillId) {
  if (skillId == 0u)
    return false;
  static uint32_t const kLangIds[] = {
      LANG_COMMON,       LANG_ORCISH,     LANG_DARNASSIAN, LANG_TAURAHE,
      LANG_DWARVISH,     LANG_THALASSIAN, LANG_GNOMISH,    LANG_TROLL,
      LANG_GUTTERSPEAK,  LANG_DRAENEI,    LANG_WORGEN,     LANG_GOBLIN,
      LANG_GNOMISH_BINARY, LANG_GOBLIN_BINARY};
  for (uint32_t lang : kLangIds) {
    if (LanguageSkillIdForLang(lang) == skillId)
      return true;
  }
  return false;
}

} // namespace

std::vector<uint32_t> BuildKnownSpells(
    uint8_t race, uint8_t klass, uint8_t level,
    PlayerCreateInfoService const &createInfo,
    ISpellDefinitionStore const *spellDefinitions,
    std::vector<uint32_t> const &extraSpellIdsFromCharacter) {
  std::vector<uint32_t> spells;
  AppendRacialLanguageSpells(race, spells);

  std::vector<uint32_t> const starterSpells =
      createInfo.GetStarterSpells(race, klass);
  if (starterSpells.empty()) {
    LOG_WARN(
        "No starter spells for race={} class={} (world DB and racial DBC).",
        static_cast<uint32_t>(race), static_cast<uint32_t>(klass));
  } else {
    spells.reserve(spells.size() + starterSpells.size());
    for (uint32_t sid : starterSpells) {
      if (IsWarlockQuestGatedSummonSpell(sid))
        continue;
      if (IsProfessionGrantSpell(spellDefinitions, sid))
        continue;
      if (IsMountOrVehicleSpell(spellDefinitions, sid))
        continue;
      if (!SpellAllowedAtLevel(spellDefinitions, sid, level))
        continue;
      PushUnique(spells, sid);
    }
  }

  FilterBySpellDbc(spellDefinitions, race, klass, spells);

  for (uint32_t sid : extraSpellIdsFromCharacter) {
    if (IsSpellFromExcludedSkillLine(sid))
      continue;
    if (IsExcludedKnownSpell(spellDefinitions, sid))
      continue;
    if (!SpellAllowedAtLevel(spellDefinitions, sid, level))
      continue;
    if (spellDefinitions && !IsLanguagePassiveSpell(sid) &&
        !spellDefinitions->HasSpell(sid))
      continue;
    PushUnique(spells, sid);
  }

  StripMountAndRidingSpells(spellDefinitions, spells);

  EnsureRacialLanguageSpells(race, spells);
  PrioritizeDefaultLanguageSpell(race, spells);

  if (ToPlayerClass(klass) == PlayerClass::Warlock) {
    spells.erase(
        std::remove_if(spells.begin(), spells.end(),
                       [](uint32_t sid) {
                         return IsWarlockQuestGatedSummonSpell(sid);
                       }),
        spells.end());
  }
  return spells;
}

std::vector<StarterSkillGrant> BuildStarterSkills(
    uint8_t race, uint8_t klass,
    PlayerCreateInfoService const &createInfo) {
  std::vector<StarterSkillGrant> out;
  std::unordered_set<uint32_t> seen;
  auto pushGrant = [&](StarterSkillGrant g, bool nativeLanguage) {
    if (g.skillId == 0u || !seen.insert(g.skillId).second)
      return;
    if (!nativeLanguage && IsExcludedStarterSkill(g.skillId))
      return;
    if (nativeLanguage) {
      g.rank = 300;
      g.maxRank = 300;
    } else {
      if (g.rank == 0u)
        g.rank = 1;
      if (g.maxRank == 0u)
        g.maxRank = g.rank;
    }
    out.push_back(g);
  };

  std::vector<uint32_t> langSkills;
  AppendRacialLanguageSkills(race, langSkills);
  for (uint32_t skillId : langSkills) {
    StarterSkillGrant g;
    g.skillId = skillId;
    pushGrant(g, true);
  }
  for (StarterSkillGrant const &g : createInfo.GetStarterSkills(race, klass))
    pushGrant(g, IsRacialLanguageSkillId(g.skillId));
  return out;
}

} // namespace PlayerSpellbook
} // namespace Firelands
