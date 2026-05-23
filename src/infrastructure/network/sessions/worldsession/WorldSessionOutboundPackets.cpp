#include <application/logic/GossipLogic.h>
#include <domain/models/Character.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/network/packets/server/GossipPackets.h>
#include <shared/Config.h>
#include <shared/game/StarterOpeningCinematic.h>
#include <shared/network/BitWriter.h>
#include <shared/network/packets/MotdPacket.h>
#include <shared/network/packets/SetProficiencyPacket.h>
#include <shared/network/packets/server/SimpleOutboundPackets.h>
#include <shared/network/packets/VerifyWorldPacket.h>
#include <shared/Logger.h>
#include <shared/network/KnownSpellsWire.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <shared/game/ActionButton.h>
#include <array>
#include <ctime>
#include <vector>

namespace Firelands {

void WorldSession::SendClientCacheVersion(uint32 version) {
  SendPacket(new WorldPackets::Misc::ClientCacheVersion(version));
}

void WorldSession::SendTutorialFlagsUnauthenticated() {
  std::array<uint32_t, Character::kTutorialMaskInts> allDone{};
  allDone.fill(0xFFFFFFFFu);
  SendTutorialMask(allDone);
}

void WorldSession::SendTutorialMask(
    std::array<uint32_t, Character::kTutorialMaskInts> const &mask) {
  WorldPacket data(SMSG_TUTORIAL_FLAGS);
  for (uint32_t word : mask)
    data.Append<uint32>(word);
  SendPacket(data);
}

void WorldSession::SendTriggerMovie(uint32_t movieId) {
  if (movieId == 0 || !_crypt.IsInitialized())
    return;
  WorldPacket pkt(SMSG_TRIGGER_MOVIE);
  pkt.Append<uint32>(movieId);
  SendPacket(pkt);
}

void WorldSession::SendTriggerCinematic(uint32_t cinematicSequenceId) {
  if (cinematicSequenceId == 0 || !_crypt.IsInitialized())
    return;
  WorldPacket pkt(SMSG_TRIGGER_CINEMATIC);
  pkt.Append<uint32>(cinematicSequenceId);
  SendPacket(pkt);
}

void WorldSession::HandleOpeningCinematic(WorldPacket &packet) {
  (void)packet;
  if (_playerGuid == 0 || _sentOpeningCinematic)
    return;
    // Cataclysm 4.3.4: repeat prompt only for characters that never gained XP.
  if (_playerXp != 0)
    return;
  auto ch = _charService->GetCharacterByGuid(_playerGuid);
  if (!ch || !ch->IsFirstLogin())
    return;
  uint32_t const seq = OpeningCinematicSequence(ch->GetClass(), ch->GetRace());
  if (seq == 0)
    return;
  SendTriggerCinematic(seq);
  _sentOpeningCinematic = true;
}

void WorldSession::HandleCompleteCinematic(WorldPacket &packet) {
  (void)packet;
}

void WorldSession::HandleNextCinematicCamera(WorldPacket &packet) {
  (void)packet;
}

void WorldSession::HandleTutorialFlag(WorldPacket &packet) {
  if (_playerGuid == 0 || packet.Size() < 4)
    return;
  uint32_t const data = packet.Read<uint32>();
  uint32_t const index = data / 32u;
  if (index >= Character::kTutorialMaskInts)
    return;
  uint32_t const bit = data % 32u;
  _tutorialInts[static_cast<size_t>(index)] |= (1u << bit);
}

void WorldSession::HandleTutorialClear(WorldPacket &packet) {
  (void)packet;
  if (_playerGuid == 0)
    return;
  _tutorialInts.fill(0xFFFFFFFFu);
  SendTutorialMask(_tutorialInts);
}

void WorldSession::HandleTutorialReset(WorldPacket &packet) {
  (void)packet;
  if (_playerGuid == 0)
    return;
  _tutorialInts.fill(0);
  SendTutorialMask(_tutorialInts);
}

void WorldSession::HandleCompleteMovie(WorldPacket &packet) {
  // Reserved for scripted post-movie hooks (reference fires script callbacks).
  (void)packet;
}

void WorldSession::SendFeatureSystemStatus() {
  WorldPacket features(SMSG_FEATURE_SYSTEM_STATUS);
  // SystemPackets.cpp: int8 ComplaintStatus (reference login uses 2)
  features.Append<int8>(2);
  features.Append<uint32>(0);
  features.Append<uint32>(0);
  features.Append<uint32>(0);
  features.Append<uint32>(0);

  BitWriter bw(features);
  for (int i = 0; i < 6; ++i) bw.WriteBit(false);
  bw.Flush();
  SendPacket(features);
}

void WorldSession::SendRealmSplit(uint32 realmId) {
  WorldPacket split(SMSG_REALM_SPLIT);
  split.Append<uint32>(realmId);
  split.Append<uint32>(0);
  BitWriter bw(split);
  bw.WriteBits(8, 7);
  bw.Flush();
  split.WriteStringNoNull("01/01/01");
  SendPacket(split);
}

void WorldSession::SendLoginSetTimeSpeed(float speed) {
  WorldPacket data(SMSG_LOGIN_SET_TIME_SPEED);
  data.AppendPackedTime(static_cast<uint32>(std::time(nullptr)));
  data.Append<float>(speed);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendLearnedDanceMoves() {
  // CharacterHandler.cpp: WorldPacket data(SMSG_LEARNED_DANCE_MOVES); data << uint64(0);
  SendPacket(new WorldPackets::Misc::LearnedDanceMoves());
}

void WorldSession::SendMotd() {
  std::vector<std::string> lines = Firelands::Config::Instance().Get<std::vector<std::string>>("Motd", {"Welcome to Firelands WoW!"});
  SendPacket(new Firelands::WorldPackets::Misc::Motd(lines));
}

void WorldSession::SendDungeonDifficulty(bool inGroup) {
  WorldPacket data(MSG_SET_DUNGEON_DIFFICULTY);
  data.Append<uint32>(0); // Difficulty::REGULAR
  data.Append<uint32>(1); // mask (matches Firelands Player::SendDungeonDifficulty)
  data.Append<uint32>(inGroup ? 1u : 0u);
  SendPacket(data);
}

void WorldSession::SendHotfixNotifyBlobEmpty() {
  SendPacket(new WorldPackets::Misc::HotfixNotifyBlobEmpty());
}

void WorldSession::SendKnownSpells(bool initialLogin,
                                   std::vector<uint32> const &spellIds) {
  WorldPacket data(SMSG_SEND_KNOWN_SPELLS);
  KnownSpellsWire::WriteSendKnownSpells(data, initialLogin, spellIds);
  SendPacket(data);
}

void WorldSession::SendLearnedSpell(uint32 spellId) {
  if (spellId == 0)
    return;
  WorldPacket data(SMSG_LEARNED_SPELL);
  data.Append<uint32>(spellId);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendUnlearnSpellsEmpty() { SendUnlearnSpells({}); }

void WorldSession::SendUnlearnSpells(std::vector<uint32> const &spellIds) {
  WorldPacket data(SMSG_SEND_UNLEARN_SPELLS);
  data.Append<uint32>(static_cast<uint32>(spellIds.size()));
  for (uint32 spellId : spellIds)
  data.Append<uint32>(spellId);
  SendPacket(data);
}

void WorldSession::SendContactListEmpty() {
  // SocialMgr.cpp PlayerSocial::SendSocialList — empty list, SOCIAL_FLAG_ALL.
  SendPacket(new WorldPackets::Social::ContactListEmpty());
}

void WorldSession::SendAllAchievementDataEmpty() {
  // AchievementMgr.cpp SendAllAchievementData — zero criteria, zero achievements.
  SendPacket(new WorldPackets::Achievement::AllDataEmpty());
}

void WorldSession::SendEquipmentSetListEmpty() {
  SendPacket(new WorldPackets::Character::EquipmentSetListEmpty());
}

void WorldSession::SendActionButtons(uint8_t reason) {
  // Cataclysm 4.3.4: SMSG_UPDATE_ACTION_BUTTONS — uint32[144] then Reason (15595 reference).
  WorldPacket data(SMSG_ACTION_BUTTONS);
  ActionButton::PackedActionBar const &bar = ActiveActionBar();
  data.Append(reinterpret_cast<uint8 const *>(bar.data()),
              ActionButton::kMaxButtons * sizeof(uint32_t));
  data.Append<uint8>(reason);

  uint32_t nonEmpty = 0;
  for (uint32_t packed : bar) {
    if (packed != 0)
      ++nonEmpty;
}
  LOG_INFO("SMSG_UPDATE_ACTION_BUTTONS reason={} nonEmptySlots={} payload={}",
           static_cast<unsigned>(reason), nonEmpty, data.Size());
  SendPacket(data);
}

void WorldSession::SendInitialActionButtons() { SendActionButtons(0); }

void WorldSession::SendInitWorldStates(uint32 mapId, uint32 zoneId, uint32 areaId) {
  WorldPacket data(SMSG_INIT_WORLD_STATES);
  data.Append<uint32>(mapId);
  data.Append<uint32>(zoneId);
  data.Append<uint32>(areaId);
  data.Append<uint16>(0);
  SendPacket(data);
}

void WorldSession::SendSetupCurrency() {
  SendPacket(new WorldPackets::Character::SetupCurrencyEmpty());
}

void WorldSession::SendClientControlUpdate(uint64 guid) {
  WorldPacket data(SMSG_CLIENT_CONTROL_UPDATE);
  data.WritePackedGuid(guid);
  data.Append<uint8>(1);
  SendPacket(data);
}

void WorldSession::SendBindPointUpdate() {
  WorldPacket data(SMSG_BIND_POINT_UPDATE);
  data.Append<float>(0.0f);
  data.Append<float>(0.0f);
  data.Append<float>(0.0f);
  data.Append<uint32>(0);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendWorldServerInfo() {
  WorldPacket data(SMSG_WORLD_SERVER_INFO);
  BitWriter bw(data);
  bw.WriteBit(false);
  bw.WriteBit(false);
  bw.WriteBit(false);
  bw.Flush();
  data.Append<uint8>(0);
  data.Append<uint32>(0);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendLoadCUFProfiles() {
  WorldPacket data(SMSG_LOAD_CUF_PROFILES);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendForcedReactions() {
  WorldPacket data(SMSG_SET_FORCED_REACTIONS);
  data.Append<uint32>(static_cast<uint32>(_forcedFactionReactions.size()));
  for (auto const &kv : _forcedFactionReactions) {
    data.Append<uint32>(kv.first);
    data.Append<uint32>(kv.second);
}
  SendPacket(data);
}

void WorldSession::SendSetProficiency(uint8 itemClass, uint32 itemMask) {
  SendPacket(new Firelands::WorldPackets::Item::SetProficiency(itemClass, itemMask));
}

void WorldSession::SendTalentsInfo() {
  // Reference: Player::SendTalentsInfoData(false) → BuildPlayerTalentsInfoData.
  // Format: uint8(isPet) | uint32(freeTalentPoints) | uint8(specsCount) |
  // uint8(activeSpec) | per-spec: uint32(primaryTree) | uint8(talentCount) |
  // uint8(MAX_GLYPH_SLOT_INDEX=6) | uint16[6] glyphs.
  // specsCount MUST be ≥ 1: with 0 the 4.3.4 client accesses specs[activeSpec]
  // out-of-bounds and crashes at the end of the loading screen.
  static constexpr uint8 kGlyphSlots = 6;
  WorldPacket data(SMSG_TALENTS_INFO);
  data.Append<uint8>(0); // isPet = false
  data.Append<uint32>(0); // freeTalentPoints
  static constexpr uint8 kSpecsCount =
      static_cast<uint8>(ActionButton::kMaxActionBarSpecs);
  data.Append<uint8>(kSpecsCount);
  data.Append<uint8>(_activeActionBarSpec);
  for (uint8 spec = 0; spec < kSpecsCount; ++spec) {
    (void)spec;
    data.Append<uint32>(0); // primaryTalentTree
  data.Append<uint8>(0); // talentIdCount
    data.Append<uint8>(kGlyphSlots);
    for (uint8 i = 0; i < kGlyphSlots; ++i)
  data.Append<uint16>(0);
}
  SendPacket(data);
}

void WorldSession::SendInitialFactions() {
  uint16 count = 256;
  WorldPacket data(SMSG_INITIALIZE_FACTIONS, 4 + count * 5);
  data.Append<uint32>(static_cast<uint32>(count));
  for (uint16 i = 0; i < count; ++i) {
  data.Append<uint8>(0);
  data.Append<uint32>(0);
}
  SendPacket(data);
}

void WorldSession::SendLoginVerifyWorld(uint32 mapId, float x, float y, float z, float o) {
  SendPacket(new Firelands::WorldPackets::Login::VerifyWorld(mapId, x, y, z, o));
}

void WorldSession::SendGossipMessage(uint64_t npcGuid, uint32_t menuId, uint32_t textId,
                                     std::vector<GossipMenuItem> const &items,
                                     std::vector<GossipQuestItem> const &quests) {
  _gossipMenuSent = true;
  auto data = gossip::BuildGossipMessage(npcGuid, menuId, textId, items, quests);
  SendPacket(data);
  SendNpcTextForGossipWindow(ResolveGossipNpcTextId(textId));
}

void WorldSession::SendGossipComplete() {
  auto data = gossip::BuildGossipComplete();
  SendPacket(data);
}

} // namespace Firelands
