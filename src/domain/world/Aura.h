#pragma once

#include <chrono>
#include <cstdint>

namespace Firelands {

class Aura {
public:
  Aura(std::uint32_t spellId, std::uint32_t auraEffectType, std::int32_t basePoints, std::int32_t dieSides,
        std::uint64_t casterGuid, std::chrono::steady_clock::time_point expireTime)
       : _spellId(spellId), _auraEffectType(auraEffectType),
         _basePoints(basePoints), _dieSides(dieSides), _casterGuid(casterGuid),
         _expireTime(expireTime) {}

  std::uint32_t GetSpellId() const { return _spellId; }
  std::uint32_t GetAuraEffectType() const { return _auraEffectType; }
  std::int32_t GetBasePoints() const { return _basePoints; }
  std::int32_t GetDieSides() const { return _dieSides; }
  std::uint64_t GetCasterGuid() const { return _casterGuid; }
  std::chrono::steady_clock::time_point GetExpireTime() const { return _expireTime; }

  bool IsExpired() const {
    return std::chrono::steady_clock::now() >= _expireTime;
  }

  std::int32_t GetMagnitude() const {
    if (_dieSides == 0)
      return _basePoints;
    return _basePoints + (_dieSides > 0 ? (rand() % _dieSides) : 0);
  }

private:
  std::uint32_t _spellId;
  std::uint32_t _auraEffectType;
  std::int32_t _basePoints;
  std::int32_t _dieSides;
  std::uint64_t _casterGuid;
  std::chrono::steady_clock::time_point _expireTime;
};

} // namespace Firelands