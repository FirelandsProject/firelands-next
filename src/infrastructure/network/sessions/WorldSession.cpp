#include <application/ports/IMapNotifier.h>
#include <application/services/WorldService.h>
#include <domain/models/Character.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionMovementChecks.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <domain/repositories/IRealmRepository.h>
#include <infrastructure/persistence/MySqlAccountDataRepository.h>
#include <shared/Config.h>
#include <shared/Logger.h>
#include <shared/network/UpdateData.h>
#include <shared/network/packets/MotdPacket.h>
#include <shared/network/packets/VerifyWorldPacket.h>
#include <shared/network/packets/SetProficiencyPacket.h>
#include <shared/network/BitReader.h>
#include <shared/network/SpellCastWire.h>
#include <shared/network/UpdateFields.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/PlayerFactionTeam.h>
#include <shared/game/PlayerGmAppearance.h>
#include <shared/dbc/GtPlayerStatGameTables.h>
#include <shared/game/WowGuid.h>
#include <shared/game/Permissions.h>
#include <domain/repositories/ICharacterRepository.h>
#include <domain/repositories/ISpellDefinitionStore.h>
#include <algorithm>
#include <map>
#include <chrono>
#include <cstring>
#include <ctime>
#include <cmath>
#include <memory>
#include <optional>
#include <optional>
#include <utility>
#include <vector>

namespace Firelands {

namespace {

std::optional<std::pair<uint32, uint32>> TryLivePlayerHealth(uint32 mapId,
                                                             uint64 guid) {
  if (guid == 0ull)
    return std::nullopt;
  auto map = WorldService::Instance().GetMap(mapId);
  if (!map)
    return std::nullopt;
  auto pl = map->TryGetPlayer(guid);
  if (!pl)
    return std::nullopt;
  return std::make_pair(pl->GetLiveHealth(), pl->GetLiveMaxHealth());
}

std::optional<std::pair<uint32, uint32>> TryLivePlayerPower1(uint32 mapId,
                                                             uint64 guid) {
  if (guid == 0ull)
    return std::nullopt;
  auto map = WorldService::Instance().GetMap(mapId);
  if (!map)
    return std::nullopt;
  auto pl = map->TryGetPlayer(guid);
  if (!pl)
    return std::nullopt;
  return std::make_pair(pl->GetLivePower1(), pl->GetLiveMaxPower1());
}

void RebuildKnownSpellIdSet(std::vector<uint32> const &ordered,
                            std::unordered_set<uint32> &outIds) {
  outIds.clear();
  outIds.reserve(ordered.size());
  for (uint32 sid : ordered)
    outIds.insert(sid);
}

constexpr uint8_t kMailMessageTypeNormal = 0;
constexpr int kMailItemEnchantSlots = 10;
constexpr uint32_t kMailDaySeconds = 86400u;
constexpr uint32_t kMailListMaxShown = 50;

void AppendOneMailListEntry(WorldPacket &data, MailInboxRow const &row,
                            std::time_t now) {
  size_t const sizePos = data.Size();
  data.Append<uint16>(0);
  size_t const payloadBegin = data.Size();

  data.Append<uint32>(static_cast<uint32>(row.mailId));
  data.Append<uint8>(kMailMessageTypeNormal);
  // Trinity `HandleGetMailList`: MAIL_NORMAL uses raw 8-byte `ObjectGuid`, not packed.
  data.Append<uint64>(MakePlayerObjectGuid(row.senderGuidLow));
  data.Append<uint64>(0u);       // COD
  data.Append<uint32>(0u);       // package
  data.Append<uint32>(41u);     // stationery (matches common TC defaults)
  data.Append<uint64>(0u);       // money
  data.Append<uint32>(row.checked);
  float daysLeft = 30.0f;
  if (row.expireTime != 0) {
    if (static_cast<uint32_t>(now) < row.expireTime) {
      daysLeft = static_cast<float>(row.expireTime - static_cast<uint32_t>(now)) /
                 static_cast<float>(kMailDaySeconds);
    } else {
      daysLeft = 0.0f;
    }
  }
  data.Append<float>(daysLeft);
  data.Append<uint32>(0u); // mail template id
  data.WriteString(row.subject);
  data.WriteString(row.body);

  size_t const itemCount =
      std::min(row.items.size(), static_cast<size_t>(12));
  data.Append<uint8>(static_cast<uint8>(itemCount));
  for (size_t i = 0; i < itemCount; ++i) {
    MailInboxItemRow const &it = row.items[i];
    data.Append<uint8>(static_cast<uint8>(i));
    data.Append<uint32>(it.itemGuidLow);
    data.Append<uint32>(it.itemEntry);
    for (int e = 0; e < kMailItemEnchantSlots; ++e) {
      data.Append<uint32>(0u);
      data.Append<uint32>(0u);
      data.Append<uint32>(0u);
    }
    data.Append<int32>(0);
    data.Append<uint32>(0u);
    uint32_t const cnt = it.count != 0 ? it.count : 1u;
    data.Append<uint32>(cnt);
    data.Append<uint32>(0u);
    data.Append<uint32>(0u);
    data.Append<uint32>(0u);
    data.Append<uint8>(1u);
  }

  uint16_t const payloadBytes =
      static_cast<uint16_t>(data.Size() - payloadBegin);
  data.PatchUInt16(sizePos, payloadBytes);
}

} // namespace

namespace ws_obj = WorldSessionObjectUpdate;

WorldSession::WorldSession(
    tcp::socket socket, std::shared_ptr<AuthService> authService,
    std::shared_ptr<CharacterService> charService,
    std::shared_ptr<ICommandService> commandService,
    std::shared_ptr<MySqlAccountDataRepository> accountDataRepo,
    std::shared_ptr<LanguagesDbc const> languagesDbc,
    std::shared_ptr<ISpellDefinitionStore const> spellDefinitions,
    std::shared_ptr<IRealmRepository> realmRepo,
    std::shared_ptr<OnlineCharacterSessionRegistry> onlineCharRegistry,
    std::shared_ptr<GmTicketService> gmTicketService,
    std::shared_ptr<ItemDbHotfixStore const> itemDbHotfix,
    std::shared_ptr<SpellManager> spellManager)
    : _socket(std::move(socket)), _authService(std::move(authService)),
      _charService(std::move(charService)),
      _commandService(std::move(commandService)),
      _accountDataRepo(std::move(accountDataRepo)),
      _languagesDbc(std::move(languagesDbc)),
      _spellDefinitions(std::move(spellDefinitions)),
      _realmRepo(std::move(realmRepo)),
      _onlineCharRegistry(std::move(onlineCharRegistry)),
      _gmTicketService(std::move(gmTicketService)),
      _itemDbHotfix(std::move(itemDbHotfix)),
      _spellManager(std::move(spellManager)), _serverSeed(0),
      _accountId(0), _timeSyncPeriodicTimer(_socket.get_executor()) {}

WorldSession::~WorldSession() {
  if (_playerGuid != 0) {
    FinalizeWorldExit();
  } else {
    UnregisterFromOnlineCharacterRegistryIfNeeded();
  }
}

void WorldSession::Start() {
  LOG_DEBUG("WorldSession started for {}", GetIpAddress());

  // Cataclysm 4.3.4 Handshake: Server sends initializer string first (NO
  // OPCODES)
  std::string initializer = "WORLD OF WARCRAFT CONNECTION - SERVER TO CLIENT";
  ByteBuffer buffer;

  // Header for the initializer: just [Size:2 (BE)], followed by the string
  // payload.
  uint16 size = static_cast<uint16>(initializer.length());
  buffer.Append<uint8>((size >> 8) & 0xFF);
  buffer.Append<uint8>(size & 0xFF);
  buffer.Append((const uint8 *)initializer.c_str(), initializer.length());

  SendPacket(buffer);
  DoRead();
}

void WorldSession::SendPacket(WorldPacket &packet) {
  _lastSentOpcode = packet.GetOpcode();
  _lastSentPayloadSize = static_cast<uint32>(packet.Size());

  ByteBuffer buffer;
  // En Cataclysm, la cabecera del servidor es [Size:2 (BE)][Opcode:2 (LE)]
  // El Size incluye los 2 bytes del Opcode.
  uint16 size = static_cast<uint16>(packet.Size() + 2);

  uint8 header[4];
  header[0] = (size >> 8) & 0xFF;
  header[1] = size & 0xFF;
  uint16 opcode = static_cast<uint16>(packet.GetOpcode());
  header[2] = opcode & 0xFF;
  header[3] = (opcode >> 8) & 0xFF;

  // En Cataclysm 4.3.4, TODA la cabecera de 4 bytes se encripta
  _crypt.EncryptSend(header, 4);

  buffer.Append(header, 4);
  if (packet.Size() > 0) {
    buffer.Append(packet.GetBuffer(), packet.Size());
  }

  LOG_DEBUG("[SMSG] {} payload={} wire={}",
            packet.GetOpcodeName(), packet.Size(), buffer.Size());

  auto shared_buffer = std::make_shared<std::vector<uint8>>(
      buffer.GetBuffer(), buffer.GetBuffer() + buffer.Size());
  QueueOutgoing(shared_buffer);
}

void WorldSession::SendPacket(ServerPacket *packet) {
  if (packet) {
    SendPacket(*const_cast<WorldPacket *>(packet->Write()));
    delete packet;
  }
}

void WorldSession::QueueOutgoing(std::shared_ptr<std::vector<uint8>> buffer) {
  _writeQueue.push_back(std::move(buffer));
  if (!_writing) {
    DoWrite();
  }
}

void WorldSession::SendPacket(ByteBuffer &buffer) {
  auto shared_buffer = std::make_shared<std::vector<uint8>>(
      buffer.GetBuffer(), buffer.GetBuffer() + buffer.Size());
  LOG_DEBUG("[SEND] raw {} bytes (handshake / non-opcode)", shared_buffer->size());
  QueueOutgoing(shared_buffer);
}

void WorldSession::DoWrite() {
  if (_writeQueue.empty()) {
    _writing = false;
    return;
  }

  _writing = true;
  auto self(shared_from_this());
  auto buffer = _writeQueue.front();
  _writeQueue.pop_front();

  boost::asio::async_write(
      _socket, boost::asio::buffer(buffer->data(), buffer->size()),
      [this, self, buffer](boost::system::error_code ec,
                           std::size_t /*length*/) {
        if (ec) {
          LOG_ERROR("[SEND] Write error: {}", ec.message());
          Close();
          return;
        }
        // Process next queued packet
        DoWrite();
      });
}

void WorldSession::SendAuthChallenge() {
  _serverSeed = static_cast<uint32>(std::rand());

  WorldPacket data(SMSG_AUTH_CHALLENGE);

  // Cata 4.3.4 (15595) SMSG_AUTH_CHALLENGE (37 bytes):
  // Estructura exacta de firelands-cata-ref:
  // [32] DosChallenge (con sobreescritura parcial)
  // [4]  Server Seed (uint32)
  // [1]  DosZeroBits (uint8)

  uint8 dosChallenge[32];
  std::memset(dosChallenge, 0, 32);

  uint8 encryptSeed[16], decryptSeed[16];
  for (int i = 0; i < 16; ++i) {
    encryptSeed[i] = static_cast<uint8>(std::rand() % 256);
    decryptSeed[i] = static_cast<uint8>(std::rand() % 256);
  }

  // Replicamos el memcpy solapado de la referencia:
  std::memcpy(&dosChallenge[0], encryptSeed, 16);
  std::memcpy(&dosChallenge[4], decryptSeed, 16); // Sobreescribe índices 4-19

  data.Append(dosChallenge, 32);
  data.Append<uint32>(_serverSeed);
  data.Append<uint8>(1);

  SendPacket(data);
}
void WorldSession::UnregisterFromOnlineCharacterRegistryIfNeeded() {
  if (!_onlineCharRegistry || _activeCharacterName.empty())
    return;
  _onlineCharRegistry->Unregister(_activeCharacterName, _playerGuid, this);
  _activeCharacterName.clear();
}

void WorldSession::Close() {
  if (_playerGuid != 0) {
    LOG_INFO("Session disconnect: Account={} IP={} Character={} (saving and removing from world)",
             _accountId, GetIpAddress(), _playerGuid);
    FinalizeWorldExit();
    LOG_DEBUG("Character removed from world: Account={} GUID={}", _accountId,
              _playerGuid);
  } else {
    UnregisterFromOnlineCharacterRegistryIfNeeded();
  }
  CancelPeriodicTimeSync();
  if (_socket.is_open()) {
    LOG_DEBUG("Closing socket for Account={} IP={}", _accountId, GetIpAddress());
    _socket.close();
  }
}

void WorldSession::CancelPeriodicTimeSync() {
  _timeSyncPeriodicTimer.cancel();
}

void WorldSession::SchedulePeriodicTimeSync() {
  auto self(shared_from_this());
  _timeSyncPeriodicTimer.expires_after(std::chrono::milliseconds(5000));
  _timeSyncPeriodicTimer.async_wait(
      [this, self](boost::system::error_code ec) {
        if (ec == boost::asio::error::operation_aborted)
          return;
        if (_playerGuid == 0)
          return;
        WorldPacket next(SMSG_TIME_SYNC_REQ);
        next.Append<uint32>(_timeSyncNextCounter++);
        SendPacket(next);
        SchedulePeriodicTimeSync();
      });
}

std::string WorldSession::GetIpAddress() const {
  try {
    return _socket.remote_endpoint().address().to_string();
  } catch (...) {
    return "unknown";
  }
}

void WorldSession::DoRead() {
  auto self(shared_from_this());
  _socket.async_read_some(
      boost::asio::buffer(_readBuffer, sizeof(_readBuffer)),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec) {

          _inBuffer.Append(_readBuffer, length);

          // Process as many packets as possible from the buffer
          while (true) {
            if (!_initialized) {
              // Handshake: unencrypted [Size:2 BE][String]
              if (_inBuffer.Size() < 2)
                break;
              uint16 size = (_inBuffer[0] << 8) | _inBuffer[1];
              if (_inBuffer.Size() < static_cast<size_t>(size + 2))
                break;

              ByteBuffer packetData;
              packetData.Append(_inBuffer.GetBuffer(), size + 2);
              HandlePacket(packetData);

              std::vector<uint8> remaining(_inBuffer.GetBuffer() + size + 2,
                                           _inBuffer.GetBuffer() +
                                               _inBuffer.Size());
              _inBuffer.Clear();
              _inBuffer.Append(remaining.data(), remaining.size());
              continue;
            }

            // Post-init: CMSG header = 6 bytes [Size:2 BE][Opcode:4 LE]
            // These 6 bytes may be ARC4-encrypted.
            if (_inBuffer.Size() < 6)
              break;

            // Decrypt header exactamente una vez por paquete (ARC4 tiene
            // estado)
            if (_crypt.IsInitialized() && !_headerDecrypted) {
              std::memcpy(_decHeader, _inBuffer.GetBuffer(), 6);
              // En Cataclysm 4.3.4, los 6 bytes de la cabecera CMSG están
              // encriptados
              _crypt.DecryptRecv(_decHeader, 6);
              _headerDecrypted = true;
            } else if (!_crypt.IsInitialized() && !_headerDecrypted) {
              std::memcpy(_decHeader, _inBuffer.GetBuffer(), 6);
              _headerDecrypted = true;
            }

            // In Cataclysm, the Size field includes the 4-byte Opcode
            uint16 pktSize = (_decHeader[0] << 8) | _decHeader[1];
            // Opcode is 4 bytes, LITTLE ENDIAN
            uint32 opcode = _decHeader[2] | (_decHeader[3] << 8) |
                            (_decHeader[4] << 16) | (_decHeader[5] << 24);

            // Total on wire: 2 (size field) + pktSize
            if (_inBuffer.Size() < static_cast<size_t>(pktSize + 2)) {
              if (_inBuffer.Size() >= 6) {
                LOG_DEBUG("Waiting for more data. Have {}, need {}",
                          _inBuffer.Size(), pktSize + 2);
              }
              break;
            }

            _headerDecrypted = false;

            uint32 payloadSize = (pktSize >= 4) ? (pktSize - 4) : 0;

            WorldPacket packet(opcode, payloadSize);
            if (payloadSize > 0) {
              packet.Append(_inBuffer.GetBuffer() + 6, payloadSize);
            }

            // Remove consumed bytes
            size_t consumed = pktSize + 2;
            std::vector<uint8> remaining(_inBuffer.GetBuffer() + consumed,
                                         _inBuffer.GetBuffer() +
                                             _inBuffer.Size());
            _inBuffer.Clear();
            _inBuffer.Append(remaining.data(), remaining.size());

            ProcessPacket(packet);
          }

          DoRead();
        } else if (ec != boost::asio::error::operation_aborted) {
          LOG_ERROR("DoRead error: {} ({})", ec.message(), ec.value());
          if (_lastSentOpcode) {
            LOG_ERROR("Last SMSG before disconnect: opcode=0x{:04X} payload={}",
                      _lastSentOpcode, _lastSentPayloadSize);
          }
          Close();
        }
      });
}

void WorldSession::HandlePacket(ByteBuffer &buffer) {
  // This method handles the initial handshake string.
  // The buffer passed here contains [Size:2 BE][String]
  if (buffer.Size() < 2)
    return;

  uint16 size = buffer.Read<uint16>();
  size = (size << 8) | (size >> 8);

  if (buffer.Size() < size)
    return;

  std::string expected = "WORLD OF WARCRAFT CONNECTION - CLIENT TO SERVER";
  std::string received;
  for (uint16 i = 0; i < size; ++i) {
    received += static_cast<char>(buffer.Read<uint8>());
  }

  while (!received.empty() &&
         (received.back() == '\0' || received.back() == '\r' ||
          received.back() == '\n')) {
    received.pop_back();
  }

  if (received == expected) {
    LOG_DEBUG("WorldSession: Handshake string validated.");
    _initialized = true;

    SendAuthChallenge();
  } else {
    LOG_ERROR("WorldSession: Invalid handshake string received. Expected '{}', "
              "got '{}'",
              expected, received);
    Close();
  }
}

// --- Client Packet Handlers (CMSG) ---

void WorldSession::HandlePlayerLogin(WorldPacket &packet) {
  uint64 guid = 0;
  LoginReadPackedPlayerGuid(packet, guid);

  _playerGuid = guid;
  _timeSyncNextCounter = 0;

  auto characterOpt = _charService->GetCharacterByGuid(guid);
  if (!characterOpt) {
    LOG_ERROR("PlayerLogin failed: Account={} GUID={} Reason=NotFound", _accountId, guid);
    Close();
    return;
  }
  Character const &character = *characterOpt;
  _playerRace = character.GetRace();
  _moneyCopper = character.GetMoney();
  _playerXp = character.GetXp();

  LOG_DEBUG("PlayerLogin: Account={} GUID={} Name='{}' Level={} Map={}",
            _accountId, guid, character.GetName(), character.GetLevel(),
            character.GetMapId());

  LoginSendAccountDataAndPreMapPackets(guid, character);
  LoginBuildKnownSpellsAndSendSpellbook(character);
  LoginSendMotdAndMetaPackets();

  MovementInfo move{};
  LoginResolveMapPosition(guid, character, move);

  LoginSpawnInWorld(guid, character, move);
  LoginSendCreateUpdatesAndMutualVisibility(guid, character, move);
  LoginFinalizeWorldEntry(guid);

  LOG_INFO("Player entered world: Account={} GUID={} Name='{}' Map={} Pos=({},{:.2},{:.2})",
           _accountId, guid, character.GetName(), character.GetMapId(),
           character.GetX(), character.GetY(), character.GetZ());
}

void WorldSession::LoginReadPackedPlayerGuid(WorldPacket &packet,
                                             uint64 &outGuid) {
  uint8 guid_bytes[8] = {0};
  BitReader br(packet);

  bool g2 = br.ReadBit();
  bool g3 = br.ReadBit();
  bool g0 = br.ReadBit();
  bool g6 = br.ReadBit();
  bool g4 = br.ReadBit();
  bool g5 = br.ReadBit();
  bool g1 = br.ReadBit();
  bool g7 = br.ReadBit();

  if (g2) guid_bytes[2] = packet.Read<uint8>() ^ 1;
  if (g7) guid_bytes[7] = packet.Read<uint8>() ^ 1;
  if (g0) guid_bytes[0] = packet.Read<uint8>() ^ 1;
  if (g3) guid_bytes[3] = packet.Read<uint8>() ^ 1;
  if (g5) guid_bytes[5] = packet.Read<uint8>() ^ 1;
  if (g6) guid_bytes[6] = packet.Read<uint8>() ^ 1;
  if (g1) guid_bytes[1] = packet.Read<uint8>() ^ 1;
  if (g4) guid_bytes[4] = packet.Read<uint8>() ^ 1;

  outGuid = 0;
  std::memcpy(&outGuid, guid_bytes, 8);
}

void WorldSession::LoginSendAccountDataAndPreMapPackets(
    uint64 guid, Character const &character) {
  // Login SMSG order aligned with firelands-cata-ref CharacterHandler::HandlePlayerLogin
  // and Player::SendInitialPacketsBeforeAddToMap (see Player.cpp ~23604).
  SendDungeonDifficulty(false);

  _activeCharacterGuid = character.GetGuid();
  if (_accountDataRepo && _preLoginPerCharAccountDirtyMask != 0) {
    for (uint32_t t = 0; t < NUM_ACCOUNT_DATA_TYPES; ++t) {
      if (!IsPerCharacterAccountDataType(t))
        continue;
      if ((_preLoginPerCharAccountDirtyMask & (1u << t)) == 0)
        continue;
      if (_accountData[t].data.empty() && _accountData[t].time == 0)
        _accountDataRepo->DeleteCharacter(_activeCharacterGuid,
                                           static_cast<uint8_t>(t));
      else
        _accountDataRepo->UpsertCharacter(
            _activeCharacterGuid, static_cast<uint8_t>(t), _accountData[t].time,
            _accountData[t].data);
    }
    _preLoginPerCharAccountDirtyMask = 0;
  }
  ReloadCharacterAccountDataFromDb(_activeCharacterGuid);
  SendAccountDataTimes(kPerCharacterAccountDataMask);
  SendLearnedDanceMoves();
  SendHotfixNotifyBlobEmpty();

  // Player::SendInitialPacketsBeforeAddToMap (same relative order as reference)
  SendClientControlUpdate(guid);
  SendBindPointUpdate();
  SendWorldServerInfo();
  SendSetProficiency(1, 0xFFFFFFFF);
  SendSetProficiency(2, 0xFFFFFFFF);
}

void WorldSession::LoginBuildKnownSpellsAndSendSpellbook(Character const &character) {
  {
    // Language passives must appear early in `SMSG_SEND_KNOWN_SPELLS`. Some 4.3.4
    // clients stop applying the list if an early spell id is unknown to the
    // client DB, which leaves language passives unlearned and blocks `/say`
    // before `CMSG_MESSAGECHAT_SAY` is ever sent.
    std::vector<uint32> spells;
    AppendRacialLanguageSpells(character.GetRace(), spells);
    std::vector<uint32_t> const fromDb = _charService->GetStarterSpells(
        static_cast<uint8_t>(character.GetRace()),
        static_cast<uint8_t>(character.GetClass()));
    auto pushUnique = [&spells](uint32 sid) {
      if (sid == 0)
        return;
      if (std::find(spells.begin(), spells.end(), sid) == spells.end())
        spells.push_back(sid);
    };
    if (!fromDb.empty()) {
      spells.reserve(spells.size() + fromDb.size());
      for (uint32_t sid : fromDb)
        pushUnique(static_cast<uint32>(sid));
    } else {
      for (uint32 sid : ws_obj::BuildDefaultKnownSpells(character.GetClass()))
        pushUnique(sid);
    }
    // Strip ids from wrong client eras that break spellbook application on 4.3.4.
    for (auto it = spells.begin(); it != spells.end();) {
      uint32 const sid = *it;
      if (sid >= 86450u && sid <= 86550u)
        it = spells.erase(it);
      else
        ++it;
    }
    if (_spellDefinitions) {
      for (auto it = spells.begin(); it != spells.end();) {
        uint32 const sid = *it;
        // Never drop passive "Language *" spells: wrong-era DBC would break /say.
        if (!IsLanguagePassiveSpell(sid) && !_spellDefinitions->HasSpell(sid)) {
          LOG_WARN(
              "Spell id {} not in Spell.dbc; omitted from known spells (race={} "
              "class={})",
              sid, static_cast<uint32>(character.GetRace()),
              static_cast<uint32>(character.GetClass()));
          it = spells.erase(it);
        } else
          ++it;
      }
    }
    for (uint32_t sid : _charService->GetCharacterSpellIds(character.GetGuid())) {
      uint32 const u = static_cast<uint32>(sid);
      if (u == 0)
        continue;
      if (_spellDefinitions && !IsLanguagePassiveSpell(u) &&
          !_spellDefinitions->HasSpell(u)) {
        continue;
      }
      if (std::find(spells.begin(), spells.end(), u) == spells.end())
        spells.push_back(u);
    }
    EnsureRacialLanguageSpells(static_cast<uint8>(character.GetRace()), spells);
    PrioritizeDefaultLanguageSpell(static_cast<uint8>(character.GetRace()),
                                   spells);
    _knownSpells = std::move(spells);
    RebuildKnownSpellIdSet(_knownSpells, _knownSpellIds);
    uint32 const defaultLang = DefaultLanguageForRace(character.GetRace());
    uint32 const defaultLangSpell = LanguageSpellIdForLang(defaultLang);
    {
      std::string ids;
      for (size_t i = 0; i < _knownSpells.size(); ++i) {
        if (i)
          ids.push_back(',');
        ids += std::to_string(_knownSpells[i]);
      }
      LOG_DEBUG("[CHAT] login race={} class={} defaultLang={} langSpell={} known={} "
                "spells={} ids=[{}]",
                static_cast<uint32>(character.GetRace()),
                static_cast<uint32>(character.GetClass()), defaultLang,
                defaultLangSpell,
                PlayerKnowsLanguage(_knownSpellIds, defaultLang) ? 1 : 0,
                _knownSpells.size(), ids);
    }
  }
  _gcdReady = {};
  _spellCooldownUntil.clear();
  _spellCategoryCooldownUntil.clear();
  // At world login this packet must initialize client spellbook state,
  // including passive language spells. Existing characters may have
  // `firstLogin = false`, but the client still expects InitialLogin=1 here.
  SendKnownSpells(true, _knownSpells);
  SendUnlearnSpellsEmpty();
  SendTalentsInfo();
  SendInitialActionButtons();
  SendInitialFactions();
  SendContactListEmpty();
  SendForcedReactions();
  SendSetupCurrency();
  SendAllAchievementDataEmpty();
  SendLoginSetTimeSpeed();
  SendEquipmentSetListEmpty();
}

void WorldSession::LoginSendMotdAndMetaPackets() {
  // Reference sends MOTD and SMSG_FEATURE_SYSTEM_STATUS after BeforeAddToMap.
  SendMotd();
  SendFeatureSystemStatus();
  SendTutorialFlags();
  SendClientCacheVersion(0);
}

void WorldSession::LoginResolveMapPosition(uint64 guid, Character const &character,
                                           MovementInfo &outMove) {
  // Create in-memory player and add to map BEFORE sending verify-world + worldstates
  _mapId = character.GetMapId();
  _zoneId = character.GetZoneId();
  static auto startTime = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  outMove.time = static_cast<uint32>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime)
          .count());
  outMove.x = character.GetX();
  outMove.y = character.GetY();
  outMove.z = character.GetZ();
  outMove.orientation = character.GetOrientation();
  if (!WsIsSaneWorldPosition(outMove)) {
    // Recover from previously persisted corrupt movement values by snapping to
    // race starter position. This avoids endless loading screens on relog.
    if (auto fallback = FallbackStartPosition(character.GetRace())) {
      _mapId = fallback->mapId;
      _zoneId = static_cast<uint16>(std::min<uint32_t>(fallback->zoneId, 0xFFFFu));
      outMove.x = fallback->x;
      outMove.y = fallback->y;
      outMove.z = fallback->z;
      outMove.orientation = fallback->orientation;
      LOG_WARN("Invalid saved position for guid {} (x={} y={} z={} o={}), "
               "using race fallback map={} zone={} x={} y={} z={} o={}",
               guid, character.GetX(), character.GetY(), character.GetZ(),
               character.GetOrientation(), _mapId, _zoneId, outMove.x, outMove.y,
               outMove.z, outMove.orientation);
    } else {
      LOG_WARN("Invalid saved position for guid {} and no race fallback; "
               "keeping DB values.", guid);
    }
  }
  // Seed session position immediately on login. If the user logs out before the
  // first movement heartbeat arrives, logout persistence must still save a valid
  // location instead of default zeros.
  _position = outMove;
}

void WorldSession::LoginSpawnInWorld(uint64 guid, Character const &character,
                                     MovementInfo const &move) {
  auto player = std::make_shared<Player>(guid, shared_from_this());
  player->SetPosition(move);
  player->SetRaceAndFaction(character.GetRace(), character.GetFactionTemplate());
  player->InitCombatResources(character.GetHealth(), character.GetMaxHealth(),
                               character.GetPower1(), character.GetMaxPower1());
  WorldService::Instance().AddPlayerToMap(_mapId, player);

  SendLoginVerifyWorld(_mapId, move.x, move.y, move.z, move.orientation);
}

void WorldSession::LoginSendCreateUpdatesAndMutualVisibility(
    uint64 guid, Character const &character, MovementInfo const &move) {
  GtPlayerStatGameTables const *const statGt = _charService->GetStatGameTables();
  uint32_t const selfNextXp =
      character.GetLevel() < 85
          ? _charService->GetXpToNextLevelForLevel(character.GetLevel())
          : 0u;
  // Now that the player is on the map, send create/update data and "after add" packets
  UpdateData update(_mapId);
  auto selfFields = ws_obj::BuildPlayerUpdateFields(
      guid, character, statGt, selfNextXp, TryLivePlayerHealth(_mapId, guid),
      TryLivePlayerPower1(_mapId, guid));
  MergeGmAppearanceIntoPlayerFields(selfFields, GetGmAppearanceForPlayerUpdates());
  update.AddCreateObject(guid, TYPEID_PLAYER, move, selfFields);

  MovementInfo itemMove{};
  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32 const itemGuidLow = character.GetVisibleItemGuidLow(slot);
    uint32 const entry = character.GetVisibleItemEntry(slot);
    if (itemGuidLow == 0 || entry == 0)
      continue;
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    update.AddCreateObject(
        itemOg, TYPEID_ITEM, itemMove,
        ws_obj::BuildItemCreateFields(itemOg, guid, entry,
                              character.GetVisibleItemStackCount(slot)));
  }
  for (size_t pi = 0; pi < kPackSlotCount; ++pi) {
    uint32 const itemGuidLow = character.GetPackItemGuidLow(pi);
    uint32 const entry = character.GetPackItemEntry(pi);
    if (itemGuidLow == 0 || entry == 0)
      continue;
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    update.AddCreateObject(
        itemOg, TYPEID_ITEM, itemMove,
        ws_obj::BuildItemCreateFields(itemOg, guid, entry,
                              character.GetPackItemStackCount(pi)));
  }

  WorldPacket updatePacket(SMSG_UPDATE_OBJECT);
  update.Build(updatePacket);
  SendPacket(updatePacket);

  // Other logged-in players see this client; this client sees them (same map).
  if (auto map = WorldService::Instance().GetMap(_mapId)) {
    map->ForEachPlayer([this, guid, &character, &move, statGt, selfNextXp](
                           std::shared_ptr<Player> const &other) {
      if (!other || other->GetGuid() == guid)
        return;
      PlayerGmAppearanceForUpdates const newPlayerGm =
          GetGmAppearanceForPlayerUpdates();
      if (auto n = other->GetNotifier()) {
        ws_obj::SendPlayerCreateToNotifier(
            n, _mapId, guid, character, move, newPlayerGm, statGt, selfNextXp,
            TryLivePlayerHealth(_mapId, guid), TryLivePlayerPower1(_mapId, guid));
      }
      if (auto otherCh = _charService->GetCharacterByGuid(other->GetGuid())) {
        PlayerGmAppearanceForUpdates otherGm{};
        if (auto ows =
                std::dynamic_pointer_cast<WorldSession>(other->GetNotifier())) {
          otherGm = ows->GetGmAppearanceForPlayerUpdates();
        }
        uint32_t const otherNext =
            otherCh->GetLevel() < 85
                ? _charService->GetXpToNextLevelForLevel(otherCh->GetLevel())
                : 0u;
        ws_obj::SendPlayerCreateToNotifier(
            std::static_pointer_cast<IMapNotifier>(shared_from_this()), _mapId,
            other->GetGuid(), *otherCh, other->GetPosition(), otherGm, statGt,
            otherNext, TryLivePlayerHealth(_mapId, other->GetGuid()),
            TryLivePlayerPower1(_mapId, other->GetGuid()));
      }
    });
  }
}

void WorldSession::LoginFinalizeWorldEntry(uint64 guid) {
  // Equivalent of Player::SendInitialPacketsAfterAddToMap: world states, then
  // ResetTimeSync + SendTimeSync (WorldSession.cpp in reference).
  SendInitWorldStates(_mapId, _zoneId);
  WorldPacket timeSync(SMSG_TIME_SYNC_REQ);
  timeSync.Append<uint32>(_timeSyncNextCounter++);
  SendPacket(timeSync);
  // Match Trinity: next time-sync is ~5s later via timer, not on each RESP.
  SchedulePeriodicTimeSync();
  SendLoadCUFProfiles();

  LOG_DEBUG("Player {} finished world entry (map {})", guid, _mapId);

  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireEvent("player_login", guid);
  }

  if (_onlineCharRegistry) {
    if (auto ch = _charService->GetCharacterByGuid(guid)) {
      _activeCharacterName = ch->GetName();
      _onlineCharRegistry->Register(
          _activeCharacterName, guid,
          std::weak_ptr<ICommandSession>(
              std::static_pointer_cast<ICommandSession>(shared_from_this())));
    }
  }

  if (_gmFlyEnabled || std::fabs(_gmRunSpeed - 7.0f) > 1e-3f)
    PublishGmMovementPacketsIfInWorld();
}

void WorldSession::FinalizeWorldExit() {
  if (_playerGuid == 0 || _accountId == 0)
    return;

  CancelPeriodicTimeSync();

  uint64 const guid = _playerGuid;
  uint32 const mapId = _mapId;

  uint32 const charGuidLow = static_cast<uint32>(guid);
  uint16 const mapIdDb =
      static_cast<uint16>(std::min<uint32_t>(_mapId, 0xFFFFu));
  uint16 const zoneIdDb =
      static_cast<uint16>(std::min<uint32_t>(_zoneId, 0xFFFFu));
  MovementInfo persistPos = _position;
  if (!WsIsSaneWorldPosition(persistPos)) {
    if (auto ch = _charService->GetCharacterByGuid(guid)) {
      persistPos.x = ch->GetX();
      persistPos.y = ch->GetY();
      persistPos.z = ch->GetZ();
      persistPos.orientation = ch->GetOrientation();
    } else {
      persistPos = MovementInfo{};
    }
  }

  std::optional<uint32_t> liveHealth;
  std::optional<uint32_t> livePower1;
  if (auto map = WorldService::Instance().GetMap(mapId)) {
    if (auto pl = map->TryGetPlayer(guid)) {
      liveHealth = pl->GetLiveHealth();
      livePower1 = pl->GetLivePower1();
    }
  }

  if (!_charService->SaveCharacterOnLogout(
          _accountId, charGuidLow, mapIdDb, zoneIdDb, persistPos.x, persistPos.y,
          persistPos.z, persistPos.orientation, _moneyCopper, _playerXp, liveHealth,
          livePower1)) {
    LOG_ERROR("SaveCharacterOnLogout failed for guid {}, account {}",
              charGuidLow, _accountId);
  } else {
    LOG_DEBUG("Saved logout position guid {}: map {} zone {} x={} y={} z={} o={}",
            charGuidLow, mapIdDb, zoneIdDb, persistPos.x, persistPos.y,
            persistPos.z, persistPos.orientation);
  }

  auto ch = _charService->GetCharacterByGuid(charGuidLow);
  if (ch) {
    auto invData = ch->GetBag0Inventory();
    if (!_charService->SaveInventory(charGuidLow, invData)) {
      LOG_ERROR("SaveInventory failed for guid {}, account {}",
                charGuidLow, _accountId);
    } else {
      LOG_DEBUG("Saved inventory for guid {}", charGuidLow);
    }
  }

  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireEvent("player_logout", guid);
  }

  WorldService::Instance().RemovePlayerFromMap(mapId, guid);

  UnregisterFromOnlineCharacterRegistryIfNeeded();

  _playerGuid = 0;
  _clientSelectionGuid = 0;
  _activeCharacterGuid = 0;
  _playerRace = 0;
  _playerXp = 0;
  _knownSpells.clear();
  _knownSpellIds.clear();
  _gcdReady = {};
  _spellCooldownUntil.clear();
  _spellCategoryCooldownUntil.clear();
  ResetGmStateForLogout();
  _mapId = 0;
  _zoneId = 0;
  _timeSyncNextCounter = 0;
  _position = MovementInfo{};
}

void WorldSession::HandleLogoutRequest(WorldPacket & /*packet*/) {
  if (_playerGuid == 0) {
    LOG_WARN("CMSG_LOGOUT_REQUEST ignored (not in world)");
    return;
  }

  // Trinity TCPP order: uint32 reason (0 = OK), uint8 instantLogout.
  // We always allow instant logout (no combat/rest model yet); client returns to
  // character selection after SMSG_LOGOUT_COMPLETE.
  WorldPacket response(SMSG_LOGOUT_RESPONSE, 5);
  response.Append<uint32>(0); // reason
  response.Append<uint8>(1);  // instant logout
  SendPacket(response);

  uint64 const guid = _playerGuid;
  FinalizeWorldExit();

  WorldPacket complete(SMSG_LOGOUT_COMPLETE, 0);
  SendPacket(complete);
  LOG_INFO("Logout: Account={} GUID={} (returned to character select)", _accountId, guid);
}

void WorldSession::HandleLogoutCancel(WorldPacket & /*packet*/) {
  // Only meaningful during a timed logout; instant logout never reaches this.
  if (_playerGuid == 0)
    return;

  WorldPacket ack(SMSG_LOGOUT_CANCEL_ACK, 0);
  SendPacket(ack);
}

void WorldSession::HandleQueryNextMailTime(WorldPacket & /*packet*/) {
  // Trinity `MailHandler.cpp::HandleQueryNextMailTime`: float is negative (e.g. -DAY)
  // when there is no pending notification; float 0 + non-zero count when unread mail
  // exists. A plain `0` float makes the client treat it as "new mail" while count 0
  // confuses the UI (minimap icon vs empty mailbox).
  constexpr float kNoMailNotificationFloat = -static_cast<float>(86400);

  if (_playerGuid == 0) {
    WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME, 8);
    data.Append<float>(kNoMailNotificationFloat);
    data.Append<uint32>(0);
    SendPacket(data);
    return;
  }

  std::time_t const now = std::time(nullptr);
  std::vector<MailInboxRow> rows =
      _charService->LoadMailInbox(static_cast<uint32_t>(_playerGuid));

  std::vector<MailInboxRow const *> unread;
  unread.reserve(rows.size());
  for (MailInboxRow const &row : rows) {
    if (row.deliverTime != 0 && static_cast<uint32_t>(now) < row.deliverTime)
      continue;
    if ((row.checked & 1u) != 0)
      continue;
    unread.push_back(&row);
  }

  if (unread.empty()) {
    WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME, 8);
    data.Append<float>(kNoMailNotificationFloat);
    data.Append<uint32>(0);
    SendPacket(data);
    return;
  }

  WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME, 256);
  data.Append<float>(0.0f);
  data.Append<uint32>(0);

  uint32_t count = 0;
  std::unordered_set<uint32_t> seenSenders;
  for (MailInboxRow const *p : unread) {
    if (!seenSenders.insert(p->senderGuidLow).second)
      continue;

    data.Append<uint64>(MakePlayerObjectGuid(p->senderGuidLow));
    data.Append<uint32>(0u);
    data.Append<uint32>(static_cast<uint32>(kMailMessageTypeNormal));
    data.Append<uint32>(41u); // stationery (DB column not present yet)
    data.Append<float>(static_cast<float>(static_cast<int64_t>(p->deliverTime) -
                                          static_cast<int64_t>(now)));
    ++count;
    if (count == 2)
      break;
  }
  data.PatchUInt32(4, count);
  SendPacket(data);
}

void WorldSession::HandleCalendarGetNumPending(WorldPacket & /*packet*/) {
  // Reference: firelands-cata-ref CalendarHandler.cpp HandleCalendarGetNumPending
  WorldPacket data(SMSG_CALENDAR_SEND_NUM_PENDING, 4);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::HandleZoneUpdate(WorldPacket &packet) {
  // Reference: firelands-cata-ref MiscHandler.cpp HandleZoneUpdateOpcode
  uint32 newZone = 0;
  if (packet.Size() - packet.GetReadPos() >= sizeof(uint32))
    newZone = packet.Read<uint32>();

  if (newZone != 0)
    _zoneId = newZone;
}

void WorldSession::HandleGuildBankRemainingWithdrawMoneyQuery(WorldPacket & /*packet*/) {
  // Reference: firelands-cata-ref GuildHandler.cpp HandleGuildBankMoneyWithdrawn
  // and Guild.cpp Guild::SendMoneyInfo → SMSG_GUILD_BANK_MONEY_WITHDRAWN(int64).
  //
  // We don't implement guilds yet → respond with 0 so the UI doesn't hang.
  WorldPacket data(SMSG_GUILD_BANK_MONEY_WITHDRAWN, 8);
  data.Append<int64>(0);
  SendPacket(data);
}

void WorldSession::HandleLfgGetStatus(WorldPacket & /*packet*/) {
  // Reference: firelands-cata-ref LFGHandler.cpp HandleLfgGetStatus
  // Minimal "not queued / not using LFG" response.
  WorldPacket data(SMSG_LFG_UPDATE_STATUS_NONE, 0);
  SendPacket(data);
}

void WorldSession::HandleLfgLockInfoRequest(WorldPacket &packet) {
  // Reference: firelands-cata-ref LFGHandler.cpp HandleLfgGetLockInfoOpcode
  // Client payload: one bit ("player" vs "party"). We parse it, but respond with
  // empty data either way for now.
  bool forPlayer = true;
  if (packet.Size() - packet.GetReadPos() >= 1) {
    BitReader br(packet);
    forPlayer = br.ReadBit();
  }

  if (forPlayer) {
    // SMSG_LFG_PLAYER_INFO:
    // - uint8  dungeonCount
    // - [dungeon entries...]
    // - uint32 blacklistCount
    // - [blacklist slots...]
    WorldPacket playerInfo(SMSG_LFG_PLAYER_INFO, 1 + 4);
    playerInfo.Append<uint8>(0);
    playerInfo.Append<uint32>(0);
    SendPacket(playerInfo);
    return;
  }

  // SMSG_LFG_PARTY_INFO:
  // - uint8 playerCount
  // - [blacklist entries...]
  WorldPacket partyInfo(SMSG_LFG_PARTY_INFO, 1);
  partyInfo.Append<uint8>(0);
  SendPacket(partyInfo);
}

void WorldSession::HandleRequestCemeteryList(WorldPacket & /*packet*/) {
  // Reference: firelands-cata-ref MiscPackets.cpp RequestCemeteryListResponse::Write
  // Layout (bit-packed):
  // - 1 bit  IsGossipTriggered
  // - 24 bits CemeteryID.size()
  // - [uint32 cemeteryId...]
  WorldPacket response(SMSG_REQUEST_CEMETERY_LIST_RESPONSE, 4);
  BitWriter bits(response);
  bits.WriteBit(false);
  bits.WriteBits(0, 24);
  bits.Flush();
  SendPacket(response);
}

void WorldSession::HandleCastSpell(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  if (!_spellManager) {
    LOG_ERROR("HandleCastSpell: SpellManager not configured");
    return;
  }

  SpellCastWire::ClientCastSpellData c;
  if (!SpellCastWire::TryReadClientCastSpell(packet, c)) {
    LOG_DEBUG(
        "CMSG_CAST_SPELL: unsupported tail or truncated packet (spellId={}, "
        "sendCastFlags=0x{:02X}, readPos={}/{})",
        c.spellId, static_cast<unsigned>(c.sendCastFlags), packet.GetReadPos(),
        packet.Size());
    return;
  }

  auto const now = std::chrono::steady_clock::now();
  SpellCastRequest req;
  req.casterGuid = _playerGuid;
  req.mapId = _mapId;
  req.client = c;
  req.now = now;
  req.gcdReady = _gcdReady;
  req.knownSpells = &_knownSpellIds;
  MovementInfo const &pos = GetPosition();
  req.hasCasterWorldPosition = true;
  req.casterX = pos.x;
  req.casterY = pos.y;
  req.casterZ = pos.z;
  if (c.unitTargetGuid != 0) {
    if (c.unitTargetGuid == _playerGuid) {
      req.hasTargetWorldPosition = true;
      req.targetX = pos.x;
      req.targetY = pos.y;
      req.targetZ = pos.z;
    } else if (auto map = WorldService::Instance().GetMap(_mapId)) {
      float tx = 0.f;
      float ty = 0.f;
      float tz = 0.f;
      if (map->TryGetObjectWorldPosition(c.unitTargetGuid, tx, ty, tz)) {
        req.hasTargetWorldPosition = true;
        req.targetX = tx;
        req.targetY = ty;
        req.targetZ = tz;
      }
      if (auto casterPl = map->TryGetPlayer(_playerGuid)) {
        if (auto targetPl = map->TryGetPlayer(c.unitTargetGuid)) {
          bool sameTeam = false;
          if (TrySpellRangeFriendlyTeamHint(casterPl->GetRace(), targetPl->GetRace(),
                                             &sameTeam)) {
            req.hasTargetFactionReactionHint = true;
            req.targetIsFriendlyTeamForSpellRange = sameTeam;
          }
        }
      }
    }
  }

  std::shared_ptr<IMapCollisionQueries> collisionHeld =
      WorldService::Instance().GetCollisionQueries();
  if (collisionHeld)
    req.collisionQueries = collisionHeld.get();

  req.spellCooldownUntilBySpellId = &_spellCooldownUntil;
  req.spellCategoryCooldownUntilByGroup = &_spellCategoryCooldownUntil;
  if (auto map = WorldService::Instance().GetMap(_mapId)) {
    if (auto casterPl = map->TryGetPlayer(_playerGuid)) {
      req.hasCasterPowerSnapshot = true;
      req.casterPower1 = casterPl->GetLivePower1();
      req.casterMaxPower1 = casterPl->GetLiveMaxPower1();
    }
  }

  SpellCastOutcome out;
  _spellManager->ProcessCastRequest(req, &out);

  switch (out.kind) {
  case SpellCastOutcome::Kind::SpellFailure:
    SendPacket(out.failurePacket);
    return;
  case SpellCastOutcome::Kind::SpellStartAndGo:
    if (auto map = WorldService::Instance().GetMap(_mapId)) {
      map->BroadcastPacketToNearby(_playerGuid, out.spellStart, true);
      map->BroadcastPacketToNearby(_playerGuid, out.spellGo, true);
      if (out.hasDirectHealthEffect && out.directHealthDelta != 0) {
        if (auto target = map->TryGetPlayer(out.directHealthTargetGuid)) {
          target->ApplyHealthDelta(out.directHealthDelta);
          WorldPacket hpUpdate;
          ws_obj::BuildPlayerHealthValuesUpdate(
              static_cast<uint16>(_mapId), out.directHealthTargetGuid,
              target->GetLiveHealth(), target->GetLiveMaxHealth(), hpUpdate);
          map->BroadcastPacketToNearby(out.directHealthTargetGuid, hpUpdate,
                                       true);
        } else if (auto cr = map->TryGetCreature(out.directHealthTargetGuid)) {
          cr->ApplyHealthDelta(out.directHealthDelta);
          WorldPacket hpUpdate;
          ws_obj::BuildPlayerHealthValuesUpdate(
              static_cast<uint16>(_mapId), out.directHealthTargetGuid,
              cr->GetLiveHealth(), cr->GetLiveMaxHealth(), hpUpdate);
          map->BroadcastPacketToNearby(out.directHealthTargetGuid, hpUpdate,
                                       true);
        }
      }
      if (out.power1Delta != 0) {
        if (auto casterPl = map->TryGetPlayer(_playerGuid)) {
          casterPl->ApplyPower1Delta(out.power1Delta);
          WorldPacket pwUpdate;
          ws_obj::BuildPlayerPower1ValuesUpdate(
              static_cast<uint16>(_mapId), _playerGuid, casterPl->GetLivePower1(),
              casterPl->GetLiveMaxPower1(), pwUpdate);
          map->BroadcastPacketToNearby(_playerGuid, pwUpdate, true);
        }
      }
      if (out.spellCooldownDurationMs > 0) {
        uint32 const sid = static_cast<uint32>(c.spellId);
        _spellCooldownUntil[sid] =
            now + std::chrono::milliseconds(
                      static_cast<int64_t>(out.spellCooldownDurationMs));
      }
      if (out.spellCategoryCooldownGroup > 0 &&
          out.spellCategoryCooldownDurationMs > 0) {
        _spellCategoryCooldownUntil[out.spellCategoryCooldownGroup] =
            now + std::chrono::milliseconds(static_cast<int64_t>(
                out.spellCategoryCooldownDurationMs));
      }
    } else {
      SendPacket(out.spellStart);
      SendPacket(out.spellGo);
    }
    _gcdReady = out.newGcdReady;
    return;
  case SpellCastOutcome::Kind::None:
  default:
    return;
  }
}

void WorldSession::HandleNameQuery(WorldPacket &packet) {
  // CMSG_NAME_QUERY: TCPP HandleNameQueryOpcode reads ObjectGuid as raw uint64 LE
  // (recvData >> guid), i.e. 8 bytes. Shorter payloads use packed GUID.
  uint64 guid = 0;
  size_t const rem = packet.Size() - packet.GetReadPos();
  if (rem >= sizeof(uint64)) {
    guid = packet.Read<uint64>();
  } else if (rem > 0) {
    guid = packet.ReadPackedGuid();
  }

  // SMSG_QUERY_PLAYER_NAME_RESPONSE: packed guid, uint8 result, optional lookup blob.
  WorldPacket response(SMSG_QUERY_PLAYER_NAME_RESPONSE);
  response.WritePackedGuid(guid);

  auto chOpt = _charService->GetCharacterByGuid(guid);
  if (!chOpt) {
    response.Append<uint8>(kQueryNameResponseFailure);
    SendPacket(response);
    return;
  }

  response.Append<uint8>(kQueryNameResponseSuccess);
  // 4.3.4 chat UI shows "Name-Realm" when this realm string is non-empty; use
  // empty so only the character name appears (RealmName in yaml is for auth/link).
  ws_obj::AppendPlayerGuidLookupData(response, *chOpt, "");
  SendPacket(response);
}

void WorldSession::SendQueryTimeResponse() {
  WorldPacket response(SMSG_QUERY_TIME_RESPONSE);
  response.Append<uint32>(static_cast<uint32>(std::time(nullptr)));
  response.Append<uint32>(0); // next daily reset (unknown/not implemented)
  SendPacket(response);
}

void WorldSession::HandleQueryTime(WorldPacket & /*packet*/) {
  SendQueryTimeResponse();
}

void WorldSession::HandlePlayedTime(WorldPacket &packet) {
  // CMSG_PLAYED_TIME: client requests /played info.
  // Payload is usually 1 byte (trigger event); our log shows size=1.
  uint8 trigger = 0;
  if (packet.Size() - packet.GetReadPos() >= 1)
    trigger = packet.Read<uint8>();

  WorldPacket response(SMSG_PLAYED_TIME);
  // TCPP WorldPackets::Character::PlayedTime::Write() uses int32 for both times.
  response.Append<int32>(0); // total played seconds (not persisted yet)
  response.Append<int32>(0); // level played seconds (not persisted yet)
  response.Append<uint8>(trigger ? 1 : 0);
  SendPacket(response);
}

void WorldSession::HandlePing(WorldPacket &packet) {
  uint32 serial = packet.Read<uint32>();
  WorldPacket pong(SMSG_PONG);
  pong.Append<uint32>(serial);
  SendPacket(pong);
}

void WorldSession::HandleSetSelection(WorldPacket &packet) {
  if (packet.Size() - packet.GetReadPos() < 1) {
    _clientSelectionGuid = 0;
    return;
  }
  _clientSelectionGuid = packet.ReadPackedGuid();
}

void WorldSession::HandleSwapInvItem(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  if (packet.Size() - packet.GetReadPos() < 2)
    return;

  uint8 dstslot = packet.Read<uint8>();
  uint8 srcslot = packet.Read<uint8>();
  if (srcslot == dstslot)
    return;

  auto validBag0Slot = [](uint8_t s) -> bool {
    return (s < EQUIPMENT_SLOT_END) ||
           (s >= INVENTORY_SLOT_ITEM_START && s < INVENTORY_SLOT_ITEM_END);
  };
  if (!validBag0Slot(srcslot) || !validBag0Slot(dstslot))
    return;

  if (!_charService->SwapBag0Slots(_playerGuid, srcslot, dstslot))
    return;

  auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
  if (!refreshed)
    return;

  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, ws_obj::BuildPlayerBag0InventoryValues(*refreshed));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
}

void WorldSession::HandleSwapItem(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  if (packet.Size() - packet.GetReadPos() < 4)
    return;

  uint8 dstbag = packet.Read<uint8>();
  uint8 dstslot = packet.Read<uint8>();
  uint8 srcbag = packet.Read<uint8>();
  uint8 srcslot = packet.Read<uint8>();

  if (srcbag != 0 || dstbag != 0)
    return;

  auto validBag0Slot = [](uint8_t s) -> bool {
    return (s < EQUIPMENT_SLOT_END) ||
           (s >= INVENTORY_SLOT_ITEM_START && s < INVENTORY_SLOT_ITEM_END);
  };
  if (!validBag0Slot(srcslot) || !validBag0Slot(dstslot))
    return;

  if (srcslot == dstslot)
    return;

  if (!_charService->SwapBag0Slots(_playerGuid, srcslot, dstslot))
    return;

  auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
  if (!refreshed)
    return;

  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, ws_obj::BuildPlayerBag0InventoryValues(*refreshed));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
}

void WorldSession::HandleTimeSyncResp(WorldPacket &packet) {
  packet.Read<uint32>(); // counter
  packet.Read<uint32>(); // clientTime
  // Trinity (MovementHandler.cpp): RESP updates clock skew only; the following
  // SMSG_TIME_SYNC_REQ is sent from SendTimeSync() on a ~5s timer — not here.
}

void WorldSession::HandleMoveTimeSkipped(WorldPacket &packet) {
  uint32 time = packet.Read<uint32>();
  BitReader br(packet);
  for (int i = 0; i < 8; ++i) { if (br.ReadBit()) packet.Read<uint8>(); }
  LOG_DEBUG("CMSG_MOVE_TIME_SKIPPED: Time: {}", time);
}

void WorldSession::HandleGossipHello(WorldPacket &packet) {
  const uint64 npcGuid = ws_obj::ReadClientTargetGuid(packet);
  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireGossipHello(npcGuid);
  }
}

void WorldSession::HandleGossipSelectOption(WorldPacket &packet) {
  const uint64 npcGuid = ws_obj::ReadClientTargetGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32) * 2 > packet.Size()) {
    return;
  }
  const uint32 menuId = packet.Read<uint32>();
  const uint32 listId = packet.Read<uint32>();
  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireGossipSelect(npcGuid, menuId, listId);
  }
}

void WorldSession::OpenGmMailboxUi() {
  if (_playerGuid == 0)
    return;
  WorldPacket data(SMSG_SHOW_MAILBOX, 32);
  data.WritePackedGuid(_playerGuid);
  SendPacket(data);
}

void WorldSession::HandleMailGetList(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  if (packet.Size() <= packet.GetReadPos())
    return;
  // 4.3.4: mailbox GUID is packed (same as `SMSG_SHOW_MAILBOX`), not a raw uint64.
  uint64 const mailboxGuid = packet.ReadPackedGuid();
  if (mailboxGuid != _playerGuid)
    return;
  if (!HasPermission(_accountAccessLevel, PrivilegeOrigin::GameClient,
                     ToMask(Permission::CommandMailbox)))
    return;
  SendMailListToClient(static_cast<uint32_t>(_playerGuid));
}

void WorldSession::SendMailListToClient(uint32_t characterGuid) {
  std::vector<MailInboxRow> rows = _charService->LoadMailInbox(characterGuid);
  std::time_t const now = std::time(nullptr);
  WorldPacket data(SMSG_MAIL_LIST_RESULT, 512);
  data.Append<uint32>(0);
  data.Append<uint8>(0);

  uint32_t shown = 0;
  for (MailInboxRow const &row : rows) {
    if (row.deliverTime != 0 && static_cast<uint32_t>(now) < row.deliverTime)
      continue;
    if (shown >= kMailListMaxShown)
      break;
    AppendOneMailListEntry(data, row, now);
    ++shown;
  }

  uint32_t const realTotal = static_cast<uint32_t>(rows.size());
  data.PatchUInt32(0, realTotal);
  data.PatchUInt8(4, static_cast<uint8>(shown));
  SendPacket(data);
}

void WorldSession::HandleRealmSplit(WorldPacket &packet) {
  uint32 unk = packet.Read<uint32>();
  WorldPacket data(SMSG_REALM_SPLIT);
  data.Append<uint32>(unk);
  data.Append<uint32>(0);
  data.WriteString("01/01/01");
  SendPacket(data);
}

// --- Server Packet Senders (SMSG) ---

void WorldSession::SendClientCacheVersion(uint32 version) {
  WorldPacket data(SMSG_CLIENTCACHE_VERSION);
  data.Append<uint32>(version);
  SendPacket(data);
}

void WorldSession::SendTutorialFlags() {
  WorldPacket data(SMSG_TUTORIAL_FLAGS);
  for (int i = 0; i < 8; ++i) data.Append<uint32>(0xFFFFFFFF);
  SendPacket(data);
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
  WorldPacket data(SMSG_LEARNED_DANCE_MOVES);
  data.Append<uint64>(0);
  SendPacket(data);
}

void WorldSession::SendMotd() {
  std::vector<std::string> lines = Firelands::Config::Instance().Get<std::vector<std::string>>("Motd", {"Welcome to Firelands WoW!"});
  SendPacket(new Firelands::WorldPackets::Misc::Motd(lines));
}

void WorldSession::SendDungeonDifficulty(bool inGroup) {
  WorldPacket data(MSG_SET_DUNGEON_DIFFICULTY);
  data.Append<uint32>(0); // Difficulty::REGULAR
  data.Append<uint32>(1); // mask (matches FirelandsCore Player::SendDungeonDifficulty)
  data.Append<uint32>(inGroup ? 1u : 0u);
  SendPacket(data);
}

void WorldSession::SendHotfixNotifyBlobEmpty() {
  WorldPacket data(SMSG_HOTFIX_NOTIFY_BLOB);
  BitWriter bw(data);
  bw.WriteBits(0, 22);
  bw.Flush();
  SendPacket(data);
}

void WorldSession::SendKnownSpells(bool initialLogin,
                                   std::vector<uint32> const &spellIds) {
  // SpellPackets.cpp SendKnownSpells::Write()
  WorldPacket data(SMSG_SEND_KNOWN_SPELLS);
  data.Append<uint8>(initialLogin ? 1u : 0u);
  data.Append<uint16>(static_cast<uint16>(spellIds.size()));
  for (uint32 spellId : spellIds) {
    data.Append<uint32>(spellId);
    data.Append<int16>(0); // Slot (unused)
  }
  data.Append<uint16>(0); // SpellHistoryEntries.size()
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

void WorldSession::SendUnlearnSpellsEmpty() {
  WorldPacket data(SMSG_SEND_UNLEARN_SPELLS);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendContactListEmpty() {
  // SocialMgr.cpp PlayerSocial::SendSocialList — empty list, SOCIAL_FLAG_ALL.
  constexpr uint32 kSocialFlagAll = 0x01u | 0x02u | 0x04u;
  WorldPacket data(SMSG_CONTACT_LIST);
  data.Append<uint32>(kSocialFlagAll);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendAllAchievementDataEmpty() {
  // AchievementMgr.cpp SendAllAchievementData — zero criteria, zero achievements.
  WorldPacket data(SMSG_ALL_ACHIEVEMENT_DATA);
  BitWriter bw(data);
  bw.WriteBits(0, 21);
  bw.WriteBits(0, 23);
  bw.Flush();
  SendPacket(data);
}

void WorldSession::SendEquipmentSetListEmpty() {
  WorldPacket data(SMSG_EQUIPMENT_SET_LIST);
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendInitialActionButtons() {
  WorldPacket data(SMSG_ACTION_BUTTONS);
  for (int i = 0; i < 144; ++i) data.Append<uint32>(0);
  data.Append<uint8>(0);
  SendPacket(data);
}

void WorldSession::SendInitWorldStates(uint32 mapId, uint32 zoneId, uint32 areaId) {
  WorldPacket data(SMSG_INIT_WORLD_STATES);
  data.Append<uint32>(mapId);
  data.Append<uint32>(zoneId);
  data.Append<uint32>(areaId);
  data.Append<uint16>(0);
  SendPacket(data);
}

void WorldSession::SendSetupCurrency() {
  WorldPacket data(SMSG_SETUP_CURRENCY);
  data.Append<uint32>(0);
  SendPacket(data);
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
  data.Append<uint32>(0);
  SendPacket(data);
}

void WorldSession::SendSetProficiency(uint8 itemClass, uint32 itemMask) {
  SendPacket(new Firelands::WorldPackets::Item::SetProficiency(itemClass, itemMask));
}

void WorldSession::SendTalentsInfo() {
  // Reference: Player::SendTalentsInfoData(false) → BuildPlayerTalentsInfoData.
  // Format: uint8(isPet) | uint32(freeTalentPoints) | uint8(specsCount) |
  //         uint8(activeSpec) | per-spec: uint32(primaryTree) | uint8(talentCount) |
  //         uint8(MAX_GLYPH_SLOT_INDEX=6) | uint16[6] glyphs.
  // specsCount MUST be ≥ 1: with 0 the 4.3.4 client accesses specs[activeSpec]
  // out-of-bounds and crashes at the end of the loading screen.
  static constexpr uint8 kGlyphSlots = 6;
  WorldPacket data(SMSG_TALENTS_INFO);
  data.Append<uint8>(0);  // isPet = false
  data.Append<uint32>(0); // freeTalentPoints
  data.Append<uint8>(1);  // specsCount = 1 (unspecialized)
  data.Append<uint8>(0);  // activeSpec = 0
  // Spec 0 block
  data.Append<uint32>(0); // primaryTalentTree = 0 (none chosen)
  data.Append<uint8>(0);  // talentIdCount = 0
  data.Append<uint8>(kGlyphSlots);
  for (uint8 i = 0; i < kGlyphSlots; ++i)
    data.Append<uint16>(0); // all glyphs empty
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

void WorldSession::PublishSelfCoinageUpdate() {
  if (_playerGuid == 0)
    return;
  uint64 const coin = _moneyCopper;
  std::map<uint16, uint32> f;
  f[PLAYER_FIELD_COINAGE] = static_cast<uint32>(coin & 0xFFFFFFFFu);
  f[static_cast<uint16>(PLAYER_FIELD_COINAGE + 1)] =
      static_cast<uint32>((coin >> 32) & 0xFFFFFFFFu);
  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, f);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
}

bool WorldSession::GmLearnSpell(uint32 spellId) {
  if (_playerGuid == 0 || spellId == 0)
    return false;
  if (_knownSpellIds.count(spellId) != 0u) {
    SendNotification("Already knows spell " + std::to_string(spellId) + ".");
    return true;
  }
  if (_spellDefinitions && !IsLanguagePassiveSpell(spellId) &&
      !_spellDefinitions->HasSpell(spellId)) {
    SendNotification("Spell id not in Spell.dbc (refused).");
    return false;
  }
  uint32 const lowGuid = static_cast<uint32>(_playerGuid);
  if (!_charService->AddCharacterSpell(lowGuid, spellId)) {
    SendNotification("Failed to persist spell.");
    return false;
  }
  _knownSpells.push_back(spellId);
  _knownSpellIds.insert(spellId);
  SendLearnedSpell(spellId);
  SendNotification("Learned spell " + std::to_string(spellId) + ".");
  return true;
}

bool WorldSession::GmModifyMoneyCopper(int64 deltaCopper) {
  if (_playerGuid == 0)
    return false;
  uint32 const lowGuid = static_cast<uint32>(_playerGuid);
  if (!_charService->AddCharacterMoneyDelta(_accountId, lowGuid, deltaCopper))
    return false;
  if (auto ch = _charService->GetCharacterByGuid(_playerGuid))
    _moneyCopper = ch->GetMoney();
  PublishSelfCoinageUpdate();
  SendNotification("Money updated (copper=" + std::to_string(_moneyCopper) + ").");
  return true;
}

bool WorldSession::GmAddItem(uint32 itemEntry, uint32 count) {
  if (_playerGuid == 0)
    return false;
  uint32 const c = std::max(1u, count);
  if (!_charService->HasItemTemplate(itemEntry)) {
    SendNotification("Item does not exist (no template for entry " +
                     std::to_string(itemEntry) + ").");
    return false;
  }
  bool mailed = false;
  uint32_t newItemGuidLow = 0;
  uint8_t newBag0Slot = 0;
  if (!_charService->GrantItemToBag0OrMail(static_cast<uint32>(_playerGuid),
                                           itemEntry, c, &mailed, &newItemGuidLow,
                                           &newBag0Slot)) {
    SendNotification("Could not add item (backpack full and mail failed, or database "
                     "error).");
    return false;
  }
  if (!mailed) {
    auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
    if (!refreshed)
      return false;
    uint32_t stackShown = c;
    if (newBag0Slot >= INVENTORY_SLOT_ITEM_START &&
        newBag0Slot < INVENTORY_SLOT_ITEM_END) {
      size_t const pi =
          static_cast<size_t>(newBag0Slot - INVENTORY_SLOT_ITEM_START);
      if (pi < kPackSlotCount)
        stackShown = refreshed->GetPackItemStackCount(pi);
    }
    UpdateData update(_mapId);
    MovementInfo itemMove{};
    uint64 const itemOg = MakeItemObjectGuid(newItemGuidLow);
    update.AddCreateObject(itemOg, TYPEID_ITEM, itemMove,
                           ws_obj::BuildItemCreateFields(itemOg, _playerGuid, itemEntry,
                                                         stackShown));
    update.AddValuesUpdate(_playerGuid,
                           ws_obj::BuildPlayerBag0InventoryValues(*refreshed));
    WorldPacket pkt(SMSG_UPDATE_OBJECT);
    update.Build(pkt);
    SendPacket(pkt);
    SendNotification("Item " + std::to_string(itemEntry) + " x" + std::to_string(c) +
                     " added to backpack.");
  } else {
    SendNotification("Backpack full; item " + std::to_string(itemEntry) + " x" +
                     std::to_string(c) +
                     " was sent to your mailbox (retrieve at a mailbox when mail is "
                     "enabled on the client).");
  }
  return true;
}

bool WorldSession::GmRemoveItem(uint32 itemEntry, uint32 count) {
  if (_playerGuid == 0)
    return false;
  uint32 const c = std::max(1u, count);
  uint32_t const removed = _charService->RemoveBag0ItemsByEntry(
      _accountId, static_cast<uint32>(_playerGuid), itemEntry, c);
  if (removed == 0) {
    SendNotification("No matching items in the main backpack (bag slots only).");
    return false;
  }
  auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
  if (!refreshed)
    return false;
  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid,
                         ws_obj::BuildPlayerBag0InventoryValues(*refreshed));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
  SendNotification("Removed item " + std::to_string(itemEntry) + " x" +
                   std::to_string(removed) + " from backpack.");
  return true;
}

bool WorldSession::GmSetLevel(uint8 level) {
  if (_playerGuid == 0)
    return false;
  uint8 lv = level;
  if (lv < 1)
    lv = 1;
  if (lv > 85)
    lv = 85;
  if (!_charService->SetCharacterLevel(_accountId, static_cast<uint32>(_playerGuid),
                                       lv)) {
    SendNotification("Failed to set level.");
    return false;
  }
  _playerXp = 0;
  std::map<uint16, uint32> f;
  f[UNIT_FIELD_LEVEL] = lv;
  if (lv < 85) {
    uint32_t const next = _charService->GetXpToNextLevelForLevel(lv);
    f[PLAYER_XP] = 0;
    f[PLAYER_NEXT_LEVEL_XP] = next;
  } else {
    f[PLAYER_XP] = 0;
    f[PLAYER_NEXT_LEVEL_XP] = 0;
  }
  UpdateData update(_mapId);
  update.AddValuesUpdate(_playerGuid, f);
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
  SendNotification("Level set to " + std::to_string(static_cast<int>(lv)) + ".");
  return true;
}

} // namespace Firelands
