#pragma once

#include <shared/Common.h>

#include <vector>

namespace Firelands {

/// Passive "Language *" spells (build 15595 / 4.3.4) so the client spell book
/// matches retail and chat uses the correct `Language` ids.
/// Reference: ChrRaces trade language + racial language; TrinityCore
/// `HandleMessagechatOpcode` language skill checks (`LanguageDesc::skill_id`).
void AppendRacialLanguageSpells(uint8 race, std::vector<uint32> &knownSpells);

/// Returns 0 if there is no default racial spell for that `Language` id.
uint32 LanguageSpellIdForLang(uint32 lang);

/// True if `knownSpells` contains the passive spell that grants `lang`.
bool PlayerKnowsLanguage(std::vector<uint32> const &knownSpells, uint32 lang);

/// Default spoken language for a race (Common/Orcish by faction in 4.3.4).
uint32 DefaultLanguageForRace(uint8 race);

/// SkillLine.dbc id for a `Language` id, or 0 when that language has no known
/// skill row in 4.3.4.
uint32 LanguageSkillIdForLang(uint32 lang);

/// Idempotent: appends every language skill that should be visible for `race`.
void AppendRacialLanguageSkills(uint8 race, std::vector<uint32> &skillIds);

/// Passive "Language *" spell ids used by this module (4.3.4). Used to avoid
/// stripping them when validating against `Spell.dbc`.
bool IsLanguagePassiveSpell(uint32 spellId);

/// Idempotent: ensures every racial language passive for `race` is present.
void EnsureRacialLanguageSpells(uint8 race, std::vector<uint32> &spellIds);

/// Moves the default `/say` language passive to the front of the list.
void PrioritizeDefaultLanguageSpell(uint8 race, std::vector<uint32> &spellIds);

} // namespace Firelands
