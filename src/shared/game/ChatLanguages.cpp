#include <shared/game/ChatLanguages.h>

#include <shared/Common.h>

#include <algorithm>

namespace Firelands {

namespace {

// Values align with `domain/models/Chat.h` `Language` (Trinity SharedDefines.h).
constexpr uint32 kLangUniversal = 0;
constexpr uint32 kLangOrcish = 1;
constexpr uint32 kLangDarnassian = 2;
constexpr uint32 kLangTaurahe = 3;
constexpr uint32 kLangDwarvish = 6;
constexpr uint32 kLangCommon = 7;
constexpr uint32 kLangThalassian = 10;
constexpr uint32 kLangGnomish = 13;
constexpr uint32 kLangTroll = 14;
constexpr uint32 kLangGutterspeak = 33;
constexpr uint32 kLangDraenei = 35;
constexpr uint32 kLangWorgen = 39;
constexpr uint32 kLangGoblin = 40;

// SkillLine.dbc language skill ids (category 10). The client uses these update
// fields for chat-frame language availability.
constexpr uint32 kSkillCommon = 98;
constexpr uint32 kSkillOrcish = 109;
constexpr uint32 kSkillDwarvish = 111;
constexpr uint32 kSkillDarnassian = 113;
constexpr uint32 kSkillTaurahe = 115;
constexpr uint32 kSkillThalassian = 137;
constexpr uint32 kSkillGnomish = 313;
constexpr uint32 kSkillTroll = 315;
constexpr uint32 kSkillGutterspeak = 673;
constexpr uint32 kSkillDraenei = 759;
constexpr uint32 kSkillGoblin = 792;

// Verified against wowhead spell titles for 4.x "Language *" passives.
constexpr uint32 kSpellCommon = 668;
constexpr uint32 kSpellOrcish = 669;
constexpr uint32 kSpellDarnassian = 671;
constexpr uint32 kSpellDwarvish = 672;
constexpr uint32 kSpellTaurahe = 670;
constexpr uint32 kSpellGnomish = 7340;
constexpr uint32 kSpellTroll = 7341;
constexpr uint32 kSpellGutterspeak = 17737;
constexpr uint32 kSpellThalassian = 813;
constexpr uint32 kSpellDraenei = 29932;
constexpr uint32 kSpellGoblin = 69269;
constexpr uint32 kSpellGilnean = 69270;

void PushUnique(std::vector<uint32> &out, uint32 spell) {
  if (spell == 0)
    return;
  if (std::find(out.begin(), out.end(), spell) == out.end())
    out.push_back(spell);
}

} // namespace

uint32 LanguageSpellIdForLang(uint32 lang) {
  switch (lang) {
  case kLangCommon:
    return kSpellCommon;
  case kLangOrcish:
    return kSpellOrcish;
  case kLangDarnassian:
    return kSpellDarnassian;
  case kLangTaurahe:
    return kSpellTaurahe;
  case kLangDwarvish:
    return kSpellDwarvish;
  case kLangGnomish:
    return kSpellGnomish;
  case kLangTroll:
    return kSpellTroll;
  case kLangGutterspeak:
    return kSpellGutterspeak;
  case kLangThalassian:
    return kSpellThalassian;
  case kLangDraenei:
    return kSpellDraenei;
  case kLangGoblin:
    return kSpellGoblin;
  case kLangWorgen:
    return kSpellGilnean;
  default:
    return 0;
  }
}

uint32 LanguageSkillIdForLang(uint32 lang) {
  switch (lang) {
  case kLangCommon:
    return kSkillCommon;
  case kLangOrcish:
    return kSkillOrcish;
  case kLangDarnassian:
    return kSkillDarnassian;
  case kLangTaurahe:
    return kSkillTaurahe;
  case kLangDwarvish:
    return kSkillDwarvish;
  case kLangGnomish:
    return kSkillGnomish;
  case kLangTroll:
    return kSkillTroll;
  case kLangGutterspeak:
    return kSkillGutterspeak;
  case kLangThalassian:
    return kSkillThalassian;
  case kLangDraenei:
    return kSkillDraenei;
  case kLangGoblin:
    return kSkillGoblin;
  case kLangWorgen:
    // Gilnean is represented by spell 69270 / language 39. SkillLine presence
    // varies across Cataclysm-era data; Common is the functional Worgen chat skill.
    return 0;
  default:
    return 0;
  }
}

bool PlayerKnowsLanguage(std::vector<uint32> const &knownSpells, uint32 lang) {
  uint32 const sid = LanguageSpellIdForLang(lang);
  if (sid == 0)
    return false;
  return std::find(knownSpells.begin(), knownSpells.end(), sid) !=
         knownSpells.end();
}

uint32 DefaultLanguageForRace(uint8 race) {
  switch (race) {
  case 2:  // Orc
  case 5:  // Undead
  case 6:  // Tauren
  case 8:  // Troll
  case 9:  // Goblin
  case 10: // Blood Elf
    return kLangOrcish;
  case 1:  // Human
  case 3:  // Dwarf
  case 4:  // Night Elf
  case 7:  // Gnome
  case 11: // Draenei
  case 22: // Worgen
    return kLangCommon;
  default:
    return kLangCommon;
  }
}

void AppendRacialLanguageSkills(uint8 race, std::vector<uint32> &skillIds) {
  std::vector<uint32> spells;
  AppendRacialLanguageSpells(race, spells);
  for (uint32 spellId : spells) {
    uint32 lang = 0;
    switch (spellId) {
    case kSpellCommon:
      lang = kLangCommon;
      break;
    case kSpellOrcish:
      lang = kLangOrcish;
      break;
    case kSpellDarnassian:
      lang = kLangDarnassian;
      break;
    case kSpellDwarvish:
      lang = kLangDwarvish;
      break;
    case kSpellTaurahe:
      lang = kLangTaurahe;
      break;
    case kSpellGnomish:
      lang = kLangGnomish;
      break;
    case kSpellTroll:
      lang = kLangTroll;
      break;
    case kSpellGutterspeak:
      lang = kLangGutterspeak;
      break;
    case kSpellThalassian:
      lang = kLangThalassian;
      break;
    case kSpellDraenei:
      lang = kLangDraenei;
      break;
    case kSpellGoblin:
      lang = kLangGoblin;
      break;
    case kSpellGilnean:
      lang = kLangWorgen;
      break;
    default:
      break;
    }
    uint32 const skill = LanguageSkillIdForLang(lang);
    if (skill != 0u)
      PushUnique(skillIds, skill);
  }
}

bool IsLanguagePassiveSpell(uint32 spellId) {
  switch (spellId) {
  case kSpellCommon:
  case kSpellOrcish:
  case kSpellDarnassian:
  case kSpellDwarvish:
  case kSpellTaurahe:
  case kSpellGnomish:
  case kSpellTroll:
  case kSpellGutterspeak:
  case kSpellThalassian:
  case kSpellDraenei:
  case kSpellGoblin:
  case kSpellGilnean:
    return true;
  default:
    return false;
  }
}

void EnsureRacialLanguageSpells(uint8 race, std::vector<uint32> &spellIds) {
  AppendRacialLanguageSpells(race, spellIds);
}

void PrioritizeDefaultLanguageSpell(uint8 race, std::vector<uint32> &spellIds) {
  uint32 const primary =
      LanguageSpellIdForLang(DefaultLanguageForRace(race));
  if (primary == 0u)
    return;
  auto it = std::find(spellIds.begin(), spellIds.end(), primary);
  if (it != spellIds.end() && it != spellIds.begin()) {
    spellIds.erase(it);
    spellIds.insert(spellIds.begin(), primary);
  }
}

void AppendRacialLanguageSpells(uint8 race, std::vector<uint32> &knownSpells) {
  switch (race) {
  case 1: // Human
    PushUnique(knownSpells, kSpellCommon);
    break;
  case 2: // Orc
    PushUnique(knownSpells, kSpellOrcish);
    break;
  case 3: // Dwarf
    PushUnique(knownSpells, kSpellDwarvish);
    PushUnique(knownSpells, kSpellCommon);
    break;
  case 4: // Night Elf
    PushUnique(knownSpells, kSpellDarnassian);
    PushUnique(knownSpells, kSpellCommon);
    break;
  case 5: // Undead
    PushUnique(knownSpells, kSpellGutterspeak);
    PushUnique(knownSpells, kSpellOrcish);
    break;
  case 6: // Tauren
    PushUnique(knownSpells, kSpellTaurahe);
    PushUnique(knownSpells, kSpellOrcish);
    break;
  case 7: // Gnome
    PushUnique(knownSpells, kSpellGnomish);
    PushUnique(knownSpells, kSpellCommon);
    break;
  case 8: // Troll
    PushUnique(knownSpells, kSpellTroll);
    PushUnique(knownSpells, kSpellOrcish);
    break;
  case 9: // Goblin
    PushUnique(knownSpells, kSpellGoblin);
    PushUnique(knownSpells, kSpellOrcish);
    break;
  case 10: // Blood Elf
    PushUnique(knownSpells, kSpellThalassian);
    PushUnique(knownSpells, kSpellOrcish);
    break;
  case 11: // Draenei
    PushUnique(knownSpells, kSpellDraenei);
    PushUnique(knownSpells, kSpellCommon);
    break;
  case 22: // Worgen
    PushUnique(knownSpells, kSpellGilnean);
    PushUnique(knownSpells, kSpellCommon);
    break;
  default:
    PushUnique(knownSpells, kSpellCommon);
    break;
  }
}

} // namespace Firelands
