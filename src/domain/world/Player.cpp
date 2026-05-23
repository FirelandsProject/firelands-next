#include <domain/world/Player.h>

namespace Firelands {

void Player::InitCombatResources(uint32 health, uint32 maxHealth, uint32 power1,
                                 uint32 maxPower1) {
  m_liveMaxHealth = std::max<uint32>(1u, maxHealth);
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

} // namespace Firelands
