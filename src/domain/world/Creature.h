#pragma once

#include <domain/world/Aura.h>
#include <domain/world/UnitAuraState.h>
#include <domain/world/WorldObject.h>
#include <shared/game/PhaseShift.h>
#include <shared/game/UnitCombatStats.h>
#include <shared/game/UnitFieldFlags.h>
#include <chrono>
#include <optional>
#include <vector>
#include <shared/Common.h>

namespace Firelands {

/// In-world unit (NPC / pet / summon). Gameplay AI is driven from Lua hooks, not
/// Smart-Script-style DB rows.
class Creature : public WorldObject {
public:
  /// When `creature_template.faction` is **0** (common on minimal SQL seeds) or a value was
  /// rejected as unknown. **Do not use 14 (Monster)** here: that made every such NPC hostile.
  /// **7** matches a typical neutral-creature template in 4.3.4 client data; set real `faction`
  /// in `creature_template` for correct per-NPC behavior.
  static constexpr uint32 kDefaultFactionTemplate = 7u;

  /// `maxHealth` seeds runtime HP from `creature_classlevelstats` / spawn bootstrap.
  Creature(uint64 guid, uint32 entry, uint32 displayId, uint32 maxHealth = 100u,
           uint8 level = 1u, uint32 factionTemplate = kDefaultFactionTemplate,
           uint32 npcFlags = 0u, uint32 unitFieldFlags = 0u, uint32 unitFieldFlags2 = 0u,
           uint32 extraFlags = 0u, float experienceModifier = 1.0f);

  uint32 GetEntry() const { return m_entry; }
  uint32 GetNpcFlags() const { return m_npcFlags; }
  /// `creature_template.unit_flags` → `UNIT_FIELD_FLAGS` on create/update.
  uint32 GetUnitFieldFlags() const { return m_unitFieldFlags; }
  uint32 GetUnitFieldFlags2() const { return m_unitFieldFlags2; }
  uint32 GetUnitDynamicFlags() const { return m_unitDynamicFlags; }
  void SetUnitDynamicFlags(uint32 dynamicFlags) { m_unitDynamicFlags = dynamicFlags; }
  bool IsDead() const { return m_liveHealth == 0; }
  bool IsInCombat() const { return (m_unitFieldFlags & kUnitFlagInCombat) != 0; }
  void MarkInCombat() { m_unitFieldFlags |= kUnitFlagInCombat; }
  void ClearInCombat() { m_unitFieldFlags &= ~kUnitFlagInCombat; }
  /// Clears combat and marks corpse lootable for the killing player.
  void MarkDeadAndLootable();
  /// `creature_template.flags_extra` (server-only template metadata).
  uint32 GetExtraFlags() const { return m_extraFlags; }
  /// Script/quest proxy units: no `UNIT_NPC_FLAGS`, not selectable, trigger extra flag.
  bool ActsAsScriptTrigger() const noexcept;
  uint32 GetDisplayId() const { return m_displayId; }
  uint8 GetLevel() const { return m_level; }
  /// `creature_template.ExperienceModifier` (1.0 = normal kill XP).
  float GetExperienceModifier() const { return m_experienceModifier; }
  /// Ensures kill XP is granted at most once per creature lifetime on the map.
  bool TryMarkKillExperienceAwarded();
  /// `FactionTemplate.dbc` row; drives client hostility vs player factions / forced reactions.
  uint32 GetFactionTemplate() const { return m_factionTemplate; }
  void SetFactionTemplate(uint32 factionTemplate);

  uint32 GetLiveHealth() const { return m_liveHealth; }
  uint32 GetLiveMaxHealth() const { return m_liveMaxHealth; }
  void ApplyHealthDelta(int32 delta);
  void SetCombatStats(UnitCombatStats stats);
  void SetPhaseShift(PhaseShift phaseShift) { m_phaseShift = std::move(phaseShift); }
  PhaseShift const &GetPhaseShift() const { return m_phaseShift; }
  UnitCombatStats const &GetCombatStats() const { return m_combatStats; }

  /// Player this creature is chasing in map combat (0 = none). Set on aggro, cleared on evade.
  uint64 GetChaseTargetPlayerGuid() const { return m_chaseTargetPlayerGuid; }
  void SetChaseTargetPlayerGuid(uint64 playerGuid) { m_chaseTargetPlayerGuid = playerGuid; }

  /// Walking back to spawn after evade; immune to player damage until reset at home.
  bool IsEvading() const { return m_isEvading; }
  void SetEvading(bool evading) { m_isEvading = evading; }
  void RestoreHealthToFull();
  /// Full reset when a creature finishes evading back at its home point.
  void CompleteEvadeAtHome();
  /// Heals toward max while evading (called from return-home movement tick).
  void TickEvadeHealthRegen(std::chrono::milliseconds interval);

  void AddAura(Aura const &aura);
  std::optional<AuraRemoval> TryRemoveAura(uint32 spellId, uint64 casterGuid = 0);
  bool HasAura(uint32 spellId) const;
  std::vector<Aura> GetActiveAuras() const;
  std::vector<AuraRemoval> UpdateAuras(std::chrono::steady_clock::time_point now);
  std::vector<AuraPeriodicTick> TickPeriodicAuras(
      std::chrono::steady_clock::time_point now);
  UnitAuraTickResult TickAuras(std::chrono::steady_clock::time_point now);
  uint8 AllocateAuraVisualSlot(uint32 spellId);

private:
  uint32 m_entry;
  uint32 m_displayId;
  uint32 m_npcFlags = 0;
  uint32 m_unitFieldFlags = 0;
  uint32 m_unitFieldFlags2 = 0;
  uint32 m_unitDynamicFlags = 0;
  uint32 m_extraFlags = 0;
  uint32 m_factionTemplate = kDefaultFactionTemplate;
  uint8 m_level = 1;
  float m_experienceModifier = 1.0f;
  bool m_killExperienceAwarded = false;
  uint32 m_liveHealth = 1;
  uint32 m_liveMaxHealth = 1;
  UnitCombatStats m_combatStats{};
  bool m_isEvading = false;
  uint64 m_chaseTargetPlayerGuid = 0;
  PhaseShift m_phaseShift;
  UnitAuraState m_auraState;
};

} // namespace Firelands
