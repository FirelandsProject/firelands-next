#pragma once

#include <domain/repositories/ISpellDefinitionStore.h>
#include <conncpp.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace Firelands {

/// Loads `Spell.dbc` (4.3.4 / build 15595) using TCPP `SpellEntryfmt` layout.
class SpellEntryDbcStore final : public ISpellDefinitionStore {
public:
  bool Load(std::string const &path);

  /// After `Load()`, merges `firelands_world.spell_dbc`: rows whose `Id` is not in
  /// DBC become full definitions (custom spells). Existing DBC ids only get
  /// `powerType` overridden when `PowerType` is non-NULL. Missing table → warn.
  void MergeSpellDbcRows(std::shared_ptr<sql::Connection> worldConn);

  bool IsLoaded() const { return m_loaded; }

  size_t DefinitionCount() const { return m_byId.size(); }

  bool HasSpell(uint32 spellId) const override;
  std::optional<SpellDefinition> GetDefinition(uint32 spellId) const override;

private:
  bool m_loaded = false;
  std::unordered_map<uint32, SpellDefinition> m_byId;
};

} // namespace Firelands
