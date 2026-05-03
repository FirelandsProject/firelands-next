#pragma once

#include <domain/repositories/ISpellDefinitionStore.h>
#include <string>
#include <unordered_map>

namespace Firelands {

/// Loads `Spell.dbc` (4.3.4 / build 15595) using TCPP `SpellEntryfmt` layout.
class SpellEntryDbcStore final : public ISpellDefinitionStore {
public:
  bool Load(std::string const &path);

  bool IsLoaded() const { return m_loaded; }

  bool HasSpell(uint32 spellId) const override;
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override;

private:
  bool m_loaded = false;
  std::unordered_map<uint32, SpellDefinition> m_byId;
};

} // namespace Firelands
