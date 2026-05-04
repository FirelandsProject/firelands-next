#include <domain/world/Player.h>

#include <algorithm>

namespace Firelands {

void Player::SetRaceAndFaction(uint8 race, uint32 factionTemplate) {
  m_race = race;
  m_factionTemplate = factionTemplate;
}

void Player::SetFactionTemplate(uint32 factionTemplate) {
  m_factionTemplate = factionTemplate;
}

void Player::InitCombatResources(uint32 health, uint32 maxHealth, uint32 power1,
                                 uint32 maxPower1) {
  m_liveMaxHealth = std::max<uint32>(1u, maxHealth);
  m_liveHealth = health;
  if (m_liveHealth > m_liveMaxHealth)
    m_liveHealth = m_liveMaxHealth;

  m_liveMaxPower1 = std::max<uint32>(1u, maxPower1);
  m_livePower1 = power1;
  if (m_livePower1 > m_liveMaxPower1)
    m_livePower1 = m_liveMaxPower1;
}

void Player::ApplyHealthDelta(int32 delta) {
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

void Player::ApplyPower1Delta(int32 delta) {
  int64 const sum =
      static_cast<int64>(m_livePower1) + static_cast<int64>(delta);
  int64 clamped = sum;
  if (clamped < 0)
    clamped = 0;
  int64 const maxP = static_cast<int64>(m_liveMaxPower1);
  if (clamped > maxP)
    clamped = maxP;
  m_livePower1 = static_cast<uint32>(clamped);
}

} // namespace Firelands
