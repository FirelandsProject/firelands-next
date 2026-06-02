#pragma once

#include <domain/ports/IMapNotifier.h>
#include <domain/world/Aura.h>
#include <domain/world/UnitAuraState.h>
#include <domain/world/WorldObject.h>
#include <shared/game/PlayerResourceRegen.h>
#include <shared/game/PhaseShift.h>
#include <shared/game/UnitCombatStats.h>
#include <array>
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
  void SetPrimaryStats(std::array<uint32, 5> const &stats) { m_primaryStats = stats; }
  std::array<uint32, 5> const &GetPrimaryStats() const { return m_primaryStats; }
  uint32 GetBaselineMaxHealth() const { return m_baselineMaxHealth; }
  void SetBaselineDodgePct(float pct) { m_baselineDodgePct = pct; }
  float GetBaselineDodgePct() const { return m_baselineDodgePct; }
  void ApplyPassiveHealthPctBonus(float healthPctBonus);
  void SetResourceRegenModifiers(ResourceRegenModifiers modifiers) {
    m_regenModifiers = modifiers;
  }
  ResourceRegenModifiers const &GetResourceRegenModifiers() const {
    return m_regenModifiers;
  }
  void SetCastHasteMultiplier(float multiplier) {
    m_castHasteMultiplier = multiplier >= 1.f ? multiplier : 1.f;
  }
  float GetCastHasteMultiplier() const { return m_castHasteMultiplier; }
  void SetKnownPermanentPassiveSpellIds(std::vector<uint32_t> spellIds) {
    m_permanentPassiveSpellIds = std::move(spellIds);
  }
  std::vector<uint32_t> const &GetKnownPermanentPassiveSpellIds() const {
    return m_permanentPassiveSpellIds;
  }
  /// Sets baseline and live stats (login / template refresh).
  void SetBaselineCombatStats(UnitCombatStats stats);
  void SetCombatStats(UnitCombatStats stats);
  UnitCombatStats const &GetCombatStats() const { return m_combatStats; }
  UnitCombatStats const &GetBaselineCombatStats() const { return m_baselineCombatStats; }
  void ApplyAuraCombatStatBonus(int32 attackPowerModPos, int32 attackPowerModNeg,
                                float attackPowerMultiplier);
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

  /// POWER1 type, spirit, and level for passive regen (`PlayerPowerType` byte).
  void InitRegenContext(uint8 powerType, uint32 spirit, uint8 level);
  uint8 GetPowerType() const { return m_powerType; }
  uint32 GetSpirit() const { return m_spirit; }
  uint8 GetLevel() const { return m_level; }
  void MarkInCombat(std::chrono::steady_clock::time_point now);
  bool IsOutOfCombatForRegen(std::chrono::steady_clock::time_point now) const;

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

  void SetPhaseShift(PhaseShift phaseShift) { m_phaseShift = std::move(phaseShift); }
  PhaseShift const &GetPhaseShift() const { return m_phaseShift; }

  void SetGmModeEnabled(bool enabled) { m_gmModeEnabled = enabled; }
  bool IsGmModeEnabled() const { return m_gmModeEnabled; }

private:
  std::shared_ptr<IMapNotifier> m_notifier;
  uint8 m_race = 0;
  uint32 m_factionTemplate = 0;
  uint32 m_liveHealth = 1;
  uint32 m_liveMaxHealth = 1;
  uint32 m_baselineMaxHealth = 1;
  float m_baselineDodgePct = 0.f;
  std::array<uint32, 5> m_primaryStats{};
  std::vector<uint32_t> m_permanentPassiveSpellIds;
  ResourceRegenModifiers m_regenModifiers{};
  float m_castHasteMultiplier = 1.f;
  uint32 m_livePower1 = 0;
  uint32 m_liveMaxPower1 = 1;
  uint32 m_liveBasePower1 = 1;
  UnitCombatStats m_baselineCombatStats{};
  UnitCombatStats m_combatStats{};
  uint8 m_powerType = 0;
  uint32 m_spirit = 0;
  uint8 m_level = 1;
  std::chrono::steady_clock::time_point m_lastCombatAt{};

  PhaseShift m_phaseShift;
  UnitAuraState m_auraState;
  bool m_gmModeEnabled = false;
};

} // namespace Firelands
