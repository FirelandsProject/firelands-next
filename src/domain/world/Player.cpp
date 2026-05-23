#include <domain/world/Player.h>

#include <shared/game/UnitCombatStats.h>

#include <algorithm>

namespace Firelands {

void Player::InitCombatResources(uint32 health, uint32 maxHealth, uint32 power1,
                                 uint32 maxPower1) {
  m_baselineMaxHealth = std::max<uint32>(1u, maxHealth);
  m_liveMaxHealth = m_baselineMaxHealth;
  m_liveHealth = health;
  if (m_liveHealth > m_liveMaxHealth)
    m_liveHealth = m_liveMaxHealth;

  m_liveMaxPower1 = std::max<uint32>(1u, maxPower1);
    m_liveBasePower1 = m_liveMaxPower1;
  m_livePower1 = power1;
  if (m_livePower1 > m_liveMaxPower1)
    m_livePower1 = m_liveMaxPower1;
}

void Player::SetRaceAndFaction(uint8 race, uint32 factionTemplate) {
  m_race = race;
  m_factionTemplate = factionTemplate;
}

void Player::SetFactionTemplate(uint32 factionTemplate) {
  m_factionTemplate = factionTemplate;
}

void Player::SetBaselineCombatStats(UnitCombatStats stats) {
  m_baselineCombatStats = stats;
  m_combatStats = stats;
}

void Player::SetCombatStats(UnitCombatStats stats) { m_combatStats = stats; }

void Player::ApplyAuraCombatStatBonus(int32 attackPowerModPos, int32 attackPowerModNeg,
                                      float attackPowerMultiplier) {
  ApplyAuraStatBonusToCombatStats(m_combatStats, attackPowerModPos, attackPowerModNeg,
                                  attackPowerMultiplier);
}

void Player::InitRegenContext(uint8 powerType, uint32 spirit, uint8 level) {
  m_powerType = powerType;
  m_spirit = spirit;
  m_level = std::max<uint8>(1, level);
}

void Player::MarkInCombat(std::chrono::steady_clock::time_point now) {
  m_lastCombatAt = now;
}

bool Player::IsOutOfCombatForRegen(
    std::chrono::steady_clock::time_point now) const {
  if (m_lastCombatAt.time_since_epoch().count() == 0)
    return true;
  return now - m_lastCombatAt >= std::chrono::seconds(5);
}

void Player::ApplyHealthDelta(int32 delta) {
  int64 const next =
      static_cast<int64>(m_liveHealth) + static_cast<int64>(delta);
  int64 clamped = next;
  if (clamped < 0)
    clamped = 0;
  int64 const maxH = static_cast<int64>(m_liveMaxHealth);
  if (clamped > maxH)
    clamped = maxH;
  m_liveHealth = static_cast<uint32>(clamped);
}

void Player::ApplyPower1Delta(int32 delta) {
  int64 const next = static_cast<int64>(m_livePower1) + static_cast<int64>(delta);
  int64 clamped = next;
  if (clamped < 0)
    clamped = 0;
  int64 const maxP = static_cast<int64>(m_liveMaxPower1);
  if (clamped > maxP)
    clamped = maxP;
  m_livePower1 = static_cast<uint32>(clamped);
}

uint8 Player::AllocateAuraVisualSlot(uint32 spellId) {
  return m_auraState.AllocateAuraVisualSlot(spellId);
}

void Player::AddAura(Aura const &aura) { m_auraState.AddAura(aura); }

void Player::RemoveAura(uint32 spellId) { m_auraState.RemoveAura(spellId); }

std::optional<AuraRemoval> Player::TryRemoveAura(uint32 spellId, uint64 casterGuid) {
  return m_auraState.TryRemoveAura(spellId, casterGuid);
}

bool Player::HasAura(uint32 spellId) const { return m_auraState.HasAura(spellId); }

std::vector<Aura> Player::GetActiveAuras() const {
  return m_auraState.GetActiveAuras();
}

std::vector<AuraRemoval> Player::UpdateAuras(
    std::chrono::steady_clock::time_point now) {
  return m_auraState.UpdateAuras(now);
}

std::vector<AuraPeriodicTick> Player::TickPeriodicAuras(
    std::chrono::steady_clock::time_point now) {
  return m_auraState.TickPeriodicAuras(now);
}

UnitAuraTickResult Player::TickAuras(std::chrono::steady_clock::time_point now) {
  return m_auraState.Tick(now);
}

void Player::ApplyPassiveHealthPctBonus(float healthPctBonus) {
  if (healthPctBonus <= 0.f)
    return;
  float const scaled =
      static_cast<float>(m_baselineMaxHealth) * (1.f + healthPctBonus);
  uint32 const newMax = static_cast<uint32>(std::max(1.f, scaled));
  m_liveMaxHealth = newMax;
  if (m_liveHealth > m_liveMaxHealth)
    m_liveHealth = m_liveMaxHealth;
}

} // namespace Firelands
