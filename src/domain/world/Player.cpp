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

void Player::AddAura(const Aura& aura) {
    m_auras[aura.GetSpellId()] = aura;
}

void Player::RemoveAura(uint32 spellId) {
    m_auras.erase(spellId);
}

bool Player::HasAura(uint32 spellId) const {
    return m_auras.find(spellId) != m_auras.end();
}

std::vector<Aura> Player::GetActiveAuras() const {
    std::vector<Aura> activeAuras;
    activeAuras.reserve(m_auras.size());
    for (const auto& pair : m_auras) {
        if (!pair.second.IsExpired()) {
            activeAuras.push_back(pair.second);
        }
    }
    return activeAuras;
}

void Player::UpdateAuras() {
    // Remove expired auras
    for (auto it = m_auras.begin(); it != m_auras.end(); ) {
        if (it->second.IsExpired()) {
            it = m_auras.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Firelands
