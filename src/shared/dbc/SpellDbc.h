#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

namespace Firelands {

/// `Spell.dbc` (client 4.3.4): row `Id` is the first uint32 of each record.
/// Used to drop starter/known spell ids that the client does not have, so
/// `SMSG_SEND_KNOWN_SPELLS` applies cleanly and language passives register.
class SpellDbc {
public:
  bool Load(std::string const &path);

  bool IsLoaded() const { return m_loaded; }

  /// True if `spellId` appears as a row Id. When not loaded, returns true
  /// (permissive) so login still works without the file.
  bool HasSpell(uint32_t spellId) const;

private:
  bool m_loaded = false;
  std::unordered_set<uint32_t> m_spellIds;
};

} // namespace Firelands
