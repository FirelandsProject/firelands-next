#pragma once

#include <application/ports/IMapNotifier.h>
#include <domain/world/WorldObject.h>
#include <domain/world/Aura.h>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Firelands {

class Player : public WorldObject {
public:
  explicit Player(uint64 guid, std::shared_ptr<IMapNotifier> notifier)
      : WorldObject(guid), m_notifier(std::move(notifier)) {}

  std::shared_ptr<IMapNotifier> GetNotifier() const { return m_notifier; }

  /// Seeded from `Character` at world login; authoritative until logout (Phase D/E).
  void InitCombatResources(uint32 health, uint32 maxHealth, uint32 power1,
                            uint32 maxPower1);
  /// Race / faction template mirror `Character` for server-side targeting hints (spell range).
  void SetRaceAndFaction(uint8 race, uint32 factionTemplate);
  void SetFactionTemplate(uint32 factionTemplate);
  uint8 GetRace() const { return m_race; }
  uint32 GetFactionTemplate() const { return m_factionTemplate; }

  uint32 GetLiveHealth() const { return m_liveHealth; }
  uint32 GetLiveMaxHealth() const { return m_liveMaxHealth; }
  void ApplyHealthDelta(int32 delta);
  uint32 GetLivePower1() const { return m_livePower1; }
  uint32 GetLiveMaxPower1() const { return m_liveMaxPower1; }
  void ApplyPower1Delta(int32 delta);

  /// Aura management
  void AddAura(const Aura& aura);
  void RemoveAura(uint32 spellId);
  bool HasAura(uint32 spellId) const;
  std::vector<Aura> GetActiveAuras() const;
  void UpdateAuras(); // Remove expired auras

private:
  std::shared_ptr<IMapNotifier> m_notifier;
  uint8 m_race = 0;
  uint32 m_factionTemplate = 0;
  uint32 m_liveHealth = 1;
  uint32 m_liveMaxHealth = 1;
  uint32 m_livePower1 = 0;
  uint32 m_liveMaxPower1 = 1;
  
  // Aura storage: spellId -> aura (assuming one aura per spell for simplicity)
  std::unordered_map<uint32_t, Aura> m_auras;
};

} // namespace Firelands
