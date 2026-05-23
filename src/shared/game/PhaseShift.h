#pragma once

#include <shared/Common.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace Firelands {

/// Cataclysm default phase id (`PhaseShift.h` in TrinityCore).
inline constexpr uint16 kDefaultPhaseId = 169u;

inline constexpr uint8 kPhaseUseFlagsNone = 0x0u;
inline constexpr uint8 kPhaseUseFlagsAlwaysVisible = 0x1u;
inline constexpr uint8 kPhaseUseFlagsInverse = 0x2u;

enum class PhaseShiftFlags : uint32 {
  None = 0x00,
  AlwaysVisible = 0x01,
  Inverse = 0x02,
  InverseUnphased = 0x04,
  Unphased = 0x08,
  NoCosmetic = 0x10,
};

struct PhaseRef {
  uint16 id = 0;
  uint32 phaseFlags = 0;
};

/// Mirrors TrinityCore `PhaseShift` visibility rules (Cataclysm PhaseId system).
struct PhaseShift {
  uint32 flags = static_cast<uint32>(PhaseShiftFlags::Unphased);
  std::vector<PhaseRef> phases;
  uint64 personalGuid = 0;

  void ClearPhases();
  void AddPhase(uint16 phaseId, uint32 phaseFlags = 0);
  bool HasPhase(uint16 phaseId) const;
  bool CanSee(PhaseShift const &other) const;
};

/// Builds spawn phase from `creature` row (`phaseUseFlags`, `PhaseId`, `PhaseGroup`).
/// `resolveGroup` returns phase ids for a `PhaseGroup` (empty → no phases from group).
void InitDbCreaturePhaseShift(
    PhaseShift &out, uint8 phaseUseFlags, uint16 phaseId, uint32 phaseGroupId,
    std::function<std::vector<uint16>(uint32 phaseGroupId)> const &resolveGroup);

} // namespace Firelands
