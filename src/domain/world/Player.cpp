#include <domain/world/Player.h>

#include <algorithm>

namespace Firelands {

void Player::InitCombatResources(uint32 health, uint32 maxHealth) {
  m_liveMaxHealth = std::max<uint32>(1u, maxHealth);
  m_liveHealth = health;
  if (m_liveHealth > m_liveMaxHealth)
    m_liveHealth = m_liveMaxHealth;
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

} // namespace Firelands
