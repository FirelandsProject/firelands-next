#pragma once

#include <application/ports/IMapNotifier.h>
#include <domain/world/Aura.h>
#include <domain/world/UnitAuraState.h>
#include <domain/world/WorldObject.h>
#include <chrono>
#include <memory>
#include <optional>
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
  /// Base POWER1 pool at login (`GetCreateMana` proxy for % spell costs).
  void SetLiveBasePower1(uint32 basePower1) { m_liveBasePower1 = basePower1; }
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
  uint32 GetLiveBasePower1() const { return m_liveBasePower1; }
  void ApplyPower1Delta(int32 delta);

  void AddAura(Aura const &aura);
  void RemoveAura(uint32 spellId);
  /// Removes by spell id when present; returns visual slot for `SMSG_AURA_UPDATE` remove.
  std::optional<AuraRemoval> TryRemoveAura(uint32 spellId, uint64 casterGuid = 0);
  bool HasAura(uint32 spellId) const;
  std::vector<Aura> GetActiveAuras() const;
  /// Removes expired auras; returns spell id + visual slot for wire remove packets.
  std::vector<AuraRemoval> UpdateAuras(std::chrono::steady_clock::time_point now);
  /// Applies due periodic ticks; advances each aura's next tick time.
  std::vector<AuraPeriodicTick> TickPeriodicAuras(
      std::chrono::steady_clock::time_point now);
  /// Expires finished auras, then periodic ticks on active ones.
  UnitAuraTickResult TickAuras(std::chrono::steady_clock::time_point now);
  /// Reuses slot for the same spell id when refreshing.
  uint8 AllocateAuraVisualSlot(uint32 spellId);

private:
  std::shared_ptr<IMapNotifier> m_notifier;
  uint8 m_race = 0;
  uint32 m_factionTemplate = 0;
  uint32 m_liveHealth = 1;
  uint32 m_liveMaxHealth = 1;
  uint32 m_livePower1 = 0;
  uint32 m_liveMaxPower1 = 1;
  uint32 m_liveBasePower1 = 1;

  UnitAuraState m_auraState;
};

} // namespace Firelands
