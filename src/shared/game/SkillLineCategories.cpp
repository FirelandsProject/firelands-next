#include <shared/game/SkillLineCategories.h>
#include <shared/game/StarterSkillFilters.h>
#include <shared/dbc/DbcReader.h>
#include <shared/Logger.h>

#include <unordered_map>

namespace Firelands {
namespace {

// 15595 client extract: 7 fields, 28-byte records (id + six uint32 columns).
constexpr char kSkillLineFmt[] = "niiiiii";

std::unordered_map<uint32_t, uint32_t> g_categoryBySkill;
std::unordered_map<uint32_t, uint32_t> g_skillLineBySpell;
bool g_loaded = false;
bool g_spellIndexLoaded = false;

} // namespace

bool LoadSkillLineCategories(std::string const &skillLineDbcPath) {
  g_categoryBySkill.clear();
  g_loaded = false;

  DbcReader reader;
  if (!reader.Load(skillLineDbcPath)) {
    LOG_WARN("SkillLine.dbc not loaded from {}; starter skills use legacy filters only.",
             skillLineDbcPath);
    return false;
  }

  std::vector<uint32_t> const offs = DbcBuildFieldByteOffsets(kSkillLineFmt);
  if (!reader.VerifyFormat(kSkillLineFmt)) {
    LOG_WARN("SkillLine.dbc format mismatch at {}", skillLineDbcPath);
    return false;
  }

  for (uint32_t rec = 0; rec < reader.GetRecordCount(); ++rec) {
    uint32_t const skillId = reader.ReadUInt32(rec, 0, offs);
    uint32_t const category = reader.ReadUInt32(rec, 1, offs);
    if (skillId != 0u)
      g_categoryBySkill[skillId] = category;
  }

  g_loaded = true;
  LOG_DEBUG("SkillLine.dbc: {} skill lines loaded for category filter.",
            g_categoryBySkill.size());
  return true;
}

bool SkillLineCategoriesLoaded() { return g_loaded; }

bool IsAllowedStarterSkillLine(uint32_t skillId) {
  if (IsSecondaryProfessionSkillLine(skillId))
    return false;
  if (!g_loaded)
    return true;
  auto it = g_categoryBySkill.find(skillId);
  if (it == g_categoryBySkill.end())
    return false;
  switch (it->second) {
  case SkillLineCategory::Weapon:
  case SkillLineCategory::Armor:
  case SkillLineCategory::Language:
    return true;
  default:
    return false;
  }
}

bool IsSecondaryProfessionSkillLine(uint32_t skillId) {
  switch (skillId) {
  case 129u: // First Aid
  case 185u: // Cooking
  case 356u: // Fishing
  case 762u: // Riding
  case 794u: // Archaeology
    return true;
  default:
    return false;
  }
}

bool IsExcludedSpellGrantSkillLine(uint32_t skillId) {
  // Mounts / companion pets / glyph meta lines (DBC category 7) must never grant
  // spells at create — ref blocks skill 777 even when SkillLine.dbc is loaded.
  if (IsMetaOrInternalStarterSkill(skillId))
    return true;
  if (IsClassSpellTabStarterSkill(skillId))
    return true;

  // Always block secondary professions regardless of DBC load state.
  if (IsSecondaryProfessionSkillLine(skillId))
    return true;

  if (!g_loaded) {
    // Fallback: block primary profession and meta skill line IDs.
    switch (skillId) {
    case 164u: case 165u: case 171u: case 182u: case 186u: case 197u:
    case 202u: case 333u: case 393u: case 773u: case 183u: case 777u:
    case 778u: case 810u:
      return true;
    default:
      return false;
    }
  }
  auto it = g_categoryBySkill.find(skillId);
  if (it == g_categoryBySkill.end())
    return false;
  switch (it->second) {
  case SkillLineCategory::Guild:      // cat 5 — guild perks (learned via guild level)
  case SkillLineCategory::Profession: // cat 11 — primary professions
  case SkillLineCategory::Generic:    // cat 12 — DND / meta
    return true;
  default:
    return false;
  }
}

bool LoadSkillLineAbilitySpellIndex(std::string const &skillLineAbilityDbcPath) {
  g_skillLineBySpell.clear();
  g_spellIndexLoaded = false;

  DbcReader reader;
  if (!reader.Load(skillLineAbilityDbcPath)) {
    LOG_WARN("SkillLineAbility.dbc not loaded from {}; excluded spell filter "
             "uses skill-line categories only.",
             skillLineAbilityDbcPath);
    return false;
  }

  constexpr char kSkillLineAbilityFmt[] = "niiiiiiiiiiiii";
  std::vector<uint32_t> const offs = DbcBuildFieldByteOffsets(kSkillLineAbilityFmt);
  if (!reader.VerifyFormat(kSkillLineAbilityFmt)) {
    LOG_WARN("SkillLineAbility.dbc format mismatch at {}", skillLineAbilityDbcPath);
    return false;
  }

  for (uint32_t rec = 0; rec < reader.GetRecordCount(); ++rec) {
    uint32_t const skillLine = reader.ReadUInt32(rec, 1, offs);
    uint32_t const spellId = reader.ReadUInt32(rec, 2, offs);
    if (spellId != 0u)
      g_skillLineBySpell[spellId] = skillLine;
  }

  g_spellIndexLoaded = true;
  LOG_DEBUG("SkillLineAbility.dbc: {} spell → skill line mappings for filters.",
            g_skillLineBySpell.size());
  return true;
}

bool IsSpellFromExcludedSkillLine(uint32_t spellId) {
  if (spellId == 0u)
    return false;
  if (g_spellIndexLoaded) {
    auto it = g_skillLineBySpell.find(spellId);
    if (it != g_skillLineBySpell.end())
      return IsExcludedSpellGrantSkillLine(it->second);
  }
  return false;
}

} // namespace Firelands
