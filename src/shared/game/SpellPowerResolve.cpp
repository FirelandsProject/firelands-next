#include <shared/game/SpellPowerResolve.h>

namespace Firelands {

namespace {

uint32 PercentOfMaxPower(uint32 maxPower1, uint32 percent) {
  if (maxPower1 == 0 || percent == 0)
    return 0;
  uint64 const scaled =
      (static_cast<uint64>(maxPower1) * static_cast<uint64>(percent)) / 100ULL;
  return static_cast<uint32>(std::min<uint64>(scaled, UINT32_MAX));
}

uint32 ClampU32(uint64 value) {
  return static_cast<uint32>(std::min<uint64>(value, UINT32_MAX));
}

} // namespace

uint32 ResolveSpellPowerCost(SpellPowerDbcRow const &row, uint32 spellPowerType,
                             uint8 casterLevel, uint8 spellLevel,
                             uint32 casterMaxPower1) {
  uint32 percent = row.costPercent;
  if (percent == 0 && row.costPercentFloat > 0.f)
    percent = static_cast<uint32>(std::floor(static_cast<double>(row.costPercentFloat) + 0.5));

  int const levelDiff =
      std::max(0, static_cast<int>(casterLevel) - static_cast<int>(std::max<uint8>(1, spellLevel)));
  uint64 cost = row.flatCost;
  if (row.costPerLevel > 0 && levelDiff > 0)
    cost += static_cast<uint64>(row.costPerLevel) * static_cast<uint64>(levelDiff);

  if (spellPowerType == 0u) {
    cost += PercentOfMaxPower(casterMaxPower1, percent);
    return ClampU32(cost);
  }

  if (percent > 0)
    cost += percent;

  if (spellPowerType == 1u)
    return ClampU32(cost / 10ULL);

  return ClampU32(cost);
}

} // namespace Firelands
