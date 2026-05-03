#include <domain/world/Creature.h>

#include <algorithm>

namespace Firelands {

Creature::Creature(uint64 guid, uint32 entry, uint32 displayId, uint32 maxHealth,
                   uint8 level)
    : WorldObject(guid), m_entry(entry), m_displayId(displayId),
      m_level(level == 0 ? 1 : level) {
  m_liveMaxHealth = std::max(1u, maxHealth);
  m_liveHealth = m_liveMaxHealth;
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

} // namespace Firelands
