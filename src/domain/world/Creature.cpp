#include <domain/world/Creature.h>

#include <shared/game/CreatureExtraFlags.h>
#include <shared/game/UnitFieldFlags.h>
#include <algorithm>

namespace Firelands {

Creature::Creature(uint64 guid, uint32 entry, uint32 displayId, uint32 maxHealth,
                   uint8 level, uint32 factionTemplate, uint32 npcFlags,
                   uint32 unitFieldFlags, uint32 unitFieldFlags2, uint32 extraFlags,
                   float experienceModifier)
    : WorldObject(guid), m_entry(entry), m_displayId(displayId), m_npcFlags(npcFlags),
      m_unitFieldFlags(unitFieldFlags), m_unitFieldFlags2(unitFieldFlags2),
      m_extraFlags(extraFlags),
      m_level(level == 0 ? 1 : level),
      m_experienceModifier(experienceModifier > 0.0f ? experienceModifier : 1.0f),
      m_factionTemplate(factionTemplate == 0 ? kDefaultFactionTemplate
                                              : factionTemplate) {
  m_liveMaxHealth = std::max(1u, maxHealth);
  m_liveHealth = m_liveMaxHealth;
}

bool Creature::ActsAsScriptTrigger() const noexcept {
  return m_npcFlags == 0u && (m_unitFieldFlags & kUnitFieldFlagNotSelectable) != 0u &&
         (m_extraFlags & kCreatureExtraFlagTrigger) != 0u;
}

bool Creature::TryMarkKillExperienceAwarded() {
  if (m_killExperienceAwarded)
    return false;
  m_killExperienceAwarded = true;
  return true;
}

void Creature::SetFactionTemplate(uint32 factionTemplate) {
  m_factionTemplate =
      factionTemplate == 0 ? kDefaultFactionTemplate : factionTemplate;
}

void Creature::MarkDeadAndLootable() {
  ClearInCombat();
  m_unitDynamicFlags = kUnitDynflagLootable | kUnitDynflagTappedByPlayer;
}

void Creature::SetCombatStats(UnitCombatStats stats) { m_combatStats = stats; }

void Creature::RestoreHealthToFull() { m_liveHealth = m_liveMaxHealth; }

void Creature::CompleteEvadeAtHome() {
  RestoreHealthToFull();
  SetEvading(false);
  SetChaseTargetPlayerGuid(0);
  ClearInCombat();
  m_unitDynamicFlags = 0;
}

void Creature::TickEvadeHealthRegen(std::chrono::milliseconds interval) {
  if (!m_isEvading || m_liveHealth >= m_liveMaxHealth)
    return;
  uint32 const gain = std::max(
      1u, static_cast<uint32>(static_cast<uint64>(m_liveMaxHealth) *
                               static_cast<uint64>(interval.count()) / 5000u));
  uint32 const next = std::min(m_liveMaxHealth, m_liveHealth + gain);
  m_liveHealth = next;
}

void Creature::ApplyHealthDelta(int32 delta) {
  int64 const sum =
      static_cast<int64>(m_liveHealth) + static_cast<int64>(delta);
  int64 clamped = sum;
  if (clamped < 0)
    clamped = 0;
  int64 const maxH = static_cast<int64>(m_liveMaxHealth);
  if (clamped > maxH)
    clamped = maxH;
  m_liveHealth = static_cast<uint32>(clamped);
}

void Creature::AddAura(Aura const &aura) { m_auraState.AddAura(aura); }

std::optional<AuraRemoval> Creature::TryRemoveAura(uint32 spellId, uint64 casterGuid) {
  return m_auraState.TryRemoveAura(spellId, casterGuid);
}

bool Creature::HasAura(uint32 spellId) const { return m_auraState.HasAura(spellId); }

std::vector<Aura> Creature::GetActiveAuras() const {
  return m_auraState.GetActiveAuras();
}

std::vector<AuraRemoval> Creature::UpdateAuras(
    std::chrono::steady_clock::time_point now) {
  return m_auraState.UpdateAuras(now);
}

std::vector<AuraPeriodicTick> Creature::TickPeriodicAuras(
    std::chrono::steady_clock::time_point now) {
  return m_auraState.TickPeriodicAuras(now);
}

UnitAuraTickResult Creature::TickAuras(std::chrono::steady_clock::time_point now) {
  return m_auraState.Tick(now);
}

uint8 Creature::AllocateAuraVisualSlot(uint32 spellId) {
  return m_auraState.AllocateAuraVisualSlot(spellId);
}

} // namespace Firelands
