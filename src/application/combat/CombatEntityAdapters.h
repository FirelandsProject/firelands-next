#pragma once

#include <domain/combat/entities/ICombatEntity.h>
#include <domain/world/Creature.h>
#include <domain/world/Player.h>
#include <memory>
#include <shared/Common.h>

namespace application::adapters {

class PlayerCombatEntity final : public ::combat::ICombatEntity {
public:
  explicit PlayerCombatEntity(std::shared_ptr<Firelands::Player> player)
      : _player(std::move(player)) {}

  uint64_t GetGuid() const override { return _player ? _player->GetGuid() : 0; }
  bool IsAlive() const override {
    return _player && _player->GetLiveHealth() > 0;
  }
  void TakeDamage(float amount) override {
    if (!_player || amount <= 0.f)
      return;
    Firelands::int32 const dmg = static_cast<Firelands::int32>(amount);
    if (dmg > 0)
      _player->ApplyHealthDelta(-dmg);
  }

  std::shared_ptr<Firelands::Player> const &GetPlayer() const { return _player; }

private:
  std::shared_ptr<Firelands::Player> _player;
};

class CreatureCombatEntity final : public ::combat::ICombatEntity {
public:
  explicit CreatureCombatEntity(std::shared_ptr<Firelands::Creature> creature)
      : _creature(std::move(creature)) {}

  uint64_t GetGuid() const override { return _creature ? _creature->GetGuid() : 0; }
  bool IsAlive() const override {
    return _creature && _creature->GetLiveHealth() > 0;
  }
  void TakeDamage(float amount) override {
    if (!_creature || amount <= 0.f || _creature->IsEvading())
      return;
    Firelands::int32 const dmg = static_cast<Firelands::int32>(amount);
    if (dmg > 0)
      _creature->ApplyHealthDelta(-dmg);
  }

  std::shared_ptr<Firelands::Creature> const &GetCreature() const { return _creature; }

private:
  std::shared_ptr<Firelands::Creature> _creature;
};

} // namespace application::adapters
