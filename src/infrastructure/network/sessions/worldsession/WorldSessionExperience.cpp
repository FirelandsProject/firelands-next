#include <application/services/PlayerCreateInfoService.h>
#include <application/services/PlayerSpellbook.h>
#include <application/services/WorldService.h>
#include <domain/world/Creature.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/ExperienceLogic.h>
#include <shared/game/RestExperienceLogic.h>
#include <shared/network/UpdateData.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <shared/network/packets/server/ExperiencePackets.h>

#include <map>

namespace Firelands {

namespace {

namespace ws_obj = WorldSessionObjectUpdate;

} // namespace

void WorldSession::MaybeGrantKillExperience(Creature &creature, uint32 hpBefore) {
  if (_playerGuid == 0 || hpBefore == 0 || creature.GetLiveHealth() != 0)
    return;
  if (_playerLevel >= ExperienceLogic::kMaxPlayerLevelCata)
    return;
  if (!creature.TryMarkKillExperienceAwarded())
    return;

  ExperienceRates const rates = WorldService::Instance().GetExperienceRates();
  uint32_t const award = ExperienceLogic::CalculateKillExperience(
      _playerLevel, creature.GetLevel(), creature.GetExperienceModifier(), rates);
  if (award == 0)
    return;

  uint32_t const nextLevelXp = _charService->GetXpToNextLevelForLevel(_playerLevel);
  RestExperienceLogic::RestConsumeResult const rest =
      RestExperienceLogic::ConsumeForKill(_playerRestBonus, award, nextLevelXp);
  _playerRestBonus = rest.restBonusAfter;

  uint32_t const totalAward = award + rest.restedBonus;

  auto xpToNext = [this](uint8_t level) -> uint32_t {
    return _charService->GetXpToNextLevelForLevel(level);
  };

  ExperienceLogic::ExperienceGainResult const gained = ExperienceLogic::ApplyExperienceGain(
      _playerLevel, _playerXp, totalAward, ExperienceLogic::kMaxPlayerLevelCata, xpToNext);

  if (!_charService->UpdateCharacterLevelAndXp(
          _accountId, static_cast<uint32_t>(_playerGuid), gained.level, gained.xp,
          _playerRestBonus)) {
    return;
  }

  _playerLevel = gained.level;
  _playerXp = gained.xp;
  PublishPlayerXpLevelUpdate(gained.level, gained.xp);
  PublishPlayerRestStateUpdate();

  WorldPacket xpGain = experience_wire::BuildLogXpGain(
      creature.GetGuid(), static_cast<int32_t>(totalAward),
      static_cast<int32_t>(award));
  SendPacket(xpGain);

  if (gained.levelsGained > 0) {
    SendNotification("You have reached level " +
                     std::to_string(static_cast<int>(gained.level)) + ".");
    if (PlayerCreateInfoService const *pci =
            _charService->GetPlayerCreateInfoService()) {
      _knownSpells = PlayerSpellbook::BuildKnownSpells(
          _playerRace, _playerClass, gained.level, *pci, _spellDefinitions.get(),
          _charService->GetCharacterSpellIds(static_cast<uint32_t>(_playerGuid)));
      _knownSpellIds.clear();
      _knownSpellIds.insert(_knownSpells.begin(), _knownSpells.end());
      SendKnownSpells(true, _knownSpells);
    }
  }
}

void WorldSession::PublishPlayerXpLevelUpdate(uint8 level, uint32 xp) {
  if (_playerGuid == 0)
    return;

  std::map<uint16, uint32> fields;
  fields[UNIT_FIELD_LEVEL] = level;
  if (level >= ExperienceLogic::kMaxPlayerLevelCata) {
    fields[PLAYER_XP] = 0;
    fields[PLAYER_NEXT_LEVEL_XP] = 0;
  } else {
    uint32_t next = _charService->GetXpToNextLevelForLevel(level);
    if (next == 0)
      next = 400u;
    uint32_t clampedXp = xp;
    if (clampedXp > next)
      clampedXp = next;
    fields[PLAYER_XP] = clampedXp;
    fields[PLAYER_NEXT_LEVEL_XP] = next;
  }

  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, fields);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
}

void WorldSession::PublishPlayerRestStateUpdate() {
  if (_playerGuid == 0)
    return;
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  ws_obj::BuildPlayerRestStateValuesUpdate(static_cast<uint16>(_mapId), _playerGuid,
                                           _playerRestBonus, _playerFacialHair, pkt);
  SendPacket(pkt);
}

void WorldSession::OnCreatureKilledByPlayer(uint64 creatureGuid, uint32 hpBefore) {
  if (_playerGuid == 0 || creatureGuid == 0)
    return;
  auto map = WorldService::Instance().GetMap(_mapId);
  if (!map)
    return;
  if (auto cr = map->TryGetCreature(creatureGuid))
    MaybeGrantKillExperience(*cr, hpBefore);
}

} // namespace Firelands
