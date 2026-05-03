#pragma once

#include <domain/models/SpellDefinition.h>
#include <optional>

namespace Firelands {

/// Port for spell rows from `Spell.dbc` (and later SQL overrides). Hot path must stay
/// O(1) and allocation-free after load.
class ISpellDefinitionStore {
public:
  virtual ~ISpellDefinitionStore() = default;

  virtual bool HasSpell(uint32 spellId) const = 0;
  virtual std::optional<SpellDefinition> GetDefinition(uint32 spellId) const = 0;
};

} // namespace Firelands
