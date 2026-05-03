#pragma once

#include <domain/repositories/ISpellDefinitionStore.h>
#include <conncpp.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace Firelands {

class ISpellCastTables;

/// Loads `Spell.dbc` (4.3.4 / build 15595) using `SpellEntryfmt` layout.
class SpellEntryDbcStore final : public ISpellDefinitionStore {
public:
  bool Load(std::string const &path);

  /// After `Load()` and optional `MergeSpellDbcRows()`, reads client `SpellEffect.dbc` and
  /// fills `immediateHealthEffectDelta` (lowest `EffectIndex` among school damage / heal with
  /// non-zero derived magnitude) plus `spellEffectHasHealKind` / `spellEffectHasHarmKind`
  /// from all effect rows (school damage, heal, health leech, environmental damage).
  void MergeImmediateHealthFromSpellEffect(std::string const &path);

  /// After `MergeImmediateHealthFromSpellEffect()` (or merge-only setups), sets `manaCost`
  /// from `SpellPower.dbc` when `spellPowerId` is non-zero. Requires `SpellCastTablesDbc::Load`.
  void ApplySpellPowerManaFromTables(ISpellCastTables const &tables);

  /// After `Load()`, merges `firelands_world.spell_dbc`: rows whose `Id` is not in
  /// DBC become full definitions (custom spells). For ids already in DBC: `PowerType`
  /// overrides when non-NULL; `OvAttributes` / `OvCastingTimeIndex` / `OvDurationIndex` /
  /// `OvRangeIndex` / `OvSchoolMask` override when non-NULL (NULL keeps DBC). If `Ov*`
  /// columns are missing (error 1054), falls back to legacy merge. Missing table → warn.
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
