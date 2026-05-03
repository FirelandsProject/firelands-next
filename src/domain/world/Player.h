#pragma once

#include <application/ports/IMapNotifier.h>
#include <domain/world/WorldObject.h>
#include <memory>

namespace Firelands {

class Player : public WorldObject {
public:
  explicit Player(uint64 guid, std::shared_ptr<IMapNotifier> notifier)
      : WorldObject(guid), m_notifier(std::move(notifier)) {}

  std::shared_ptr<IMapNotifier> GetNotifier() const { return m_notifier; }

  /// Seeded from `Character` at world login; authoritative until logout (Phase D).
  void InitCombatResources(uint32 health, uint32 maxHealth);
  uint32 GetLiveHealth() const { return m_liveHealth; }
  uint32 GetLiveMaxHealth() const { return m_liveMaxHealth; }
  void ApplyHealthDelta(int32 delta);

private:
  std::shared_ptr<IMapNotifier> m_notifier;
  uint32 m_liveHealth = 1;
  uint32 m_liveMaxHealth = 1;
};

} // namespace Firelands
