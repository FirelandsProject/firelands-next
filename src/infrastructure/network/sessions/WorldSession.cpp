#include <application/services/WorldService.h>
#include <domain/models/Character.h>
#include <domain/models/Chat.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/Config.h>
#include <shared/Crypto.h>
#include <shared/Logger.h>
#include <shared/network/UpdateData.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/packets/MotdPacket.h>
#include <shared/network/packets/VerifyWorldPacket.h>
#include <shared/network/packets/SetProficiencyPacket.h>
#include <ctime>
#include <chrono>
#include <vector>

namespace Firelands {

namespace {

/// Starter spells per class (4.3.4), from firelands-cata-ref `playercreateinfo_spell_custom.sql`
/// (classmask 1<<class-1). Keeps the client spell book non-empty so InitialLogin does not
/// leave null pointers (WoW #132 at address 0x4).
std::vector<uint32> BuildDefaultKnownSpells(uint8 classId) {
  switch (classId) {
  case 1: // Warrior
    return {2457, 71, 78, 100, 6673, 772, 3127, 34428};
  case 2: // Paladin
    return {465, 635, 20154, 20271, 19740, 498, 633, 82242};
  case 3: // Hunter
    return {75, 13165, 1978, 3044, 56641, 781, 1130, 2973};
  case 4: // Rogue
    return {1784, 2098, 53, 1752, 921, 1766, 1776, 82245};
  case 5: // Priest
    return {585, 589, 2061, 17, 139, 2050, 8092, 86475};
  case 6: // Death Knight
    return {48263, 45524, 49998, 47528, 48721, 45529, 48792, 86471};
  case 7: // Shaman
    return {331, 8042, 8017, 8050, 324, 51730, 5185, 52127};
  case 8: // Mage
    return {116, 133, 2136, 1459, 130, 1953, 118, 86473};
  case 9: // Warlock
    return {172, 348, 687, 1454, 5782, 980, 603, 86478};
  case 11: // Druid
    return {8921, 5185, 774, 768, 1126, 339, 467, 86470};
  default:
    return {6673, 78, 2457, 3127};
  }
}

// TCPP SharedDefines.h + QueryPackets.cpp (WorldPackets::Query::QueryPlayerNameResponse)
enum QueryNameResponseCode : uint8 {
  RESPONSE_SUCCESS = 0, // full PlayerGuidLookupData after result byte
  RESPONSE_FAILURE = 1, // only packed guid + result (client keeps “unknown” name)
};

// ByteBuffer& operator<<(ByteBuffer&, PlayerGuidLookupData const&) — same field order.
static void AppendPlayerGuidLookupData(WorldPacket &dst, Character const &ch,
                                       std::string const &realmName) {
  dst.WriteString(ch.GetName());
  dst.WriteString(realmName);
  dst.Append<uint8>(ch.GetRace());
  dst.Append<uint8>(ch.GetGender());
  dst.Append<uint8>(ch.GetClass());
  dst.Append<uint8>(0); // DeclinedNames.has_value() == false
}

static uint64 ReadClientTargetGuid(WorldPacket &packet) {
  const size_t rem = packet.Size() - packet.GetReadPos();
  if (rem >= sizeof(uint64)) {
    return packet.Read<uint64>();
  }
  if (rem > 0) {
    return packet.ReadPackedGuid();
  }
  return 0;
}

static bool IsClientMovementOpcode(WorldOpcode opcode) {
  switch (opcode) {
  case MSG_MOVE_HEARTBEAT:
  case MSG_MOVE_START_FORWARD:
  case MSG_MOVE_START_BACKWARD:
  case MSG_MOVE_STOP:
  case MSG_MOVE_SET_FACING:
    return true;
  default:
    return false;
  }
}

} // namespace

WorldSession::WorldSession(tcp::socket socket,
                           std::shared_ptr<AuthService> authService,
                           std::shared_ptr<CharacterService> charService,
                           std::shared_ptr<ICommandService> commandService)
    : _socket(std::move(socket)), _authService(std::move(authService)),
      _charService(std::move(charService)),
      _commandService(std::move(commandService)), _serverSeed(0),
      _accountId(0) {}

void WorldSession::Start() {
  LOG_INFO("WorldSession started for {}", GetIpAddress());

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

  LOG_DEBUG("[SMSG] opcode=0x{:04X} payload={} total_on_wire={}",
            static_cast<uint32>(packet.GetOpcode()), packet.Size(),
            static_cast<uint32>(buffer.Size()));

  SendPacket(buffer);
}

void WorldSession::SendPacket(ServerPacket *packet) {
  if (packet) {
    SendPacket(*const_cast<WorldPacket *>(packet->Write()));
    delete packet;
  }
}

void WorldSession::SendPacket(ByteBuffer &buffer) {
  auto shared_buffer = std::make_shared<std::vector<uint8>>(
      buffer.GetBuffer(), buffer.GetBuffer() + buffer.Size());

  // Log hex of every outgoing packet for diagnostics
  {
    std::string hexDump;
    for (size_t i = 0; i < shared_buffer->size() && i < 64; ++i) {
      char hex[4];
      std::snprintf(hex, sizeof(hex), "%02X ", (*shared_buffer)[i]);
      hexDump += hex;
    }
    if (shared_buffer->size() > 64)
      hexDump += "...";
    LOG_INFO("[SEND] {} bytes: {}", shared_buffer->size(), hexDump);
  }

  // Queue the buffer and start writing if not already in progress
  _writeQueue.push_back(shared_buffer);
  if (!_writing) {
    DoWrite();
  }
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
  LOG_INFO("SMSG_AUTH_CHALLENGE sent (ServerSeed: 0x{:08X})", _serverSeed);
}
void WorldSession::Close() {
  if (_socket.is_open()) {
    LOG_INFO("Closing WorldSession for {}", GetIpAddress());
    _socket.close();
  }
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

            LOG_INFO("[DEBUG] Decrypted Header: Size {}, Opcode 0x{:08X}",
                     pktSize, opcode);

            // Total on wire: 2 (size field) + pktSize
            if (_inBuffer.Size() < static_cast<size_t>(pktSize + 2)) {
              if (_inBuffer.Size() >= 6) {
                LOG_DEBUG("Waiting for more data. Have {}, need {}",
                          _inBuffer.Size(), pktSize + 2);
              }
              break;
            }

            _headerDecrypted = false;

            LOG_INFO("[PKT] Received: 0x{:04X} (Decrypted: {}), Size: {}",
                     opcode, _crypt.IsInitialized(), pktSize);

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
    LOG_INFO("WorldSession: Handshake string validated.");
    _initialized = true;

    SendAuthChallenge();
  } else {
    LOG_ERROR("WorldSession: Invalid handshake string received. Expected '{}', "
              "got '{}'",
              expected, received);
    Close();
  }
}

void WorldSession::ProcessPacket(WorldPacket &packet) {
  uint32 opcode = packet.GetOpcode();
  LOG_INFO("WorldSession received packet: {}, size: {}", packet.GetOpcodeName(),
           packet.Size());

  switch (opcode) {
  case CMSG_AUTH_SESSION:
    HandleAuthSession(packet);
    break;
  case CMSG_CHAR_CREATE:
    HandleCharCreate(packet);
    break;
  case CMSG_CHAR_DELETE:
    HandleCharDelete(packet);
    break;
  case CMSG_CHAR_ENUM:
    HandleCharEnum(packet);
    break;
  case CMSG_SET_ACTIVE_MOVER:
  case CMSG_SET_ACTIONBAR_TOGGLES:
  case CMSG_REQUEST_RAID_INFO:
  case CMSG_GMTICKET_GETTICKET:
  case CMSG_UNREGISTER_ALL_ADDON_PREFIXES:
  case MSG_QUERY_NEXT_MAIL_TIME:
  case CMSG_BATTLEFIELD_STATUS:
  case CMSG_QUERY_BATTLEFIELD_STATE:
  case CMSG_LFG_GET_STATUS:
  case CMSG_LFG_LOCK_INFO_REQUEST:
  case CMSG_GUILD_BANK_REMAINING_WITHDRAW_MONEY_QUERY:
  case CMSG_CALENDAR_GET_NUM_PENDING:
  case CMSG_VOICE_SESSION_ENABLE:
  case CMSG_GUILD_SET_ACHIEVEMENT_TRACKING:
  case CMSG_REQUEST_CATEGORY_COOLDOWNS:
  case CMSG_DB_QUERY_BULK:
  case CMSG_WORLD_STATE_UI_TIMER_UPDATE:
  case CMSG_ZONEUPDATE:
  case CMSG_REQUEST_CEMETERY_LIST:
    // Client probes features we haven't implemented yet. For stability we safely
    // ignore these requests (no side effects, no disconnect).
    break;
  case CMSG_LOADING_SCREEN_NOTIFY:
    // Simply acknowledge loading screen progress
    break;
  case CMSG_LOG_DISCONNECT:
    LOG_INFO("Client disconnected (CMSG_LOG_DISCONNECT)");
    Close();
    break;
  case CMSG_MESSAGECHAT:
    HandleMessageChat(packet);
    break;
  case CMSG_GOSSIP_HELLO:
    HandleGossipHello(packet);
    break;
  case CMSG_GOSSIP_SELECT_OPTION:
    HandleGossipSelectOption(packet);
    break;
  case CMSG_NAME_QUERY:
    HandleNameQuery(packet);
    break;
  case CMSG_QUERY_TIME:
    HandleQueryTime(packet);
    break;
  case CMSG_PLAYED_TIME:
    HandlePlayedTime(packet);
    break;
  case CMSG_MOVE_TIME_SKIPPED:
    HandleMoveTimeSkipped(packet);
    break;
  case CMSG_PING:
    HandlePing(packet);
    break;
  case CMSG_PLAYER_LOGIN:
    HandlePlayerLogin(packet);
    break;
  case CMSG_READY_FOR_ACCOUNT_DATA_TIMES:
    HandleReadyForAccountDataTimes(packet);
    break;
  case CMSG_REALM_SPLIT:
    HandleRealmSplit(packet);
    break;
  case CMSG_TIME_SYNC_RESP:
    HandleTimeSyncResp(packet);
    break;
  case CMSG_UPDATE_ACCOUNT_DATA:
    HandleUpdateAccountData(packet);
    break;
  case CMSG_CANCEL_TRADE:
    // Client sends this opportunistically (e.g. UI cleanup on login). Safe no-op.
    break;
  case CMSG_VIOLENCE_LEVEL:
    // Ignore violence level settings
    break;
  case MSG_MOVE_HEARTBEAT:
  case MSG_MOVE_START_FORWARD:
  case MSG_MOVE_START_BACKWARD:
  case MSG_MOVE_STOP:
  case MSG_MOVE_SET_FACING:
  case MSG_MOVE_FALL_LAND:
    HandleMovement(packet);
    break;
  default:
    LOG_DEBUG("[PACKET] Unknown/unhandled opcode: 0x{:04X} (size: {})", opcode,
              packet.Size());
    break;
  }
}

// --- Client Packet Handlers (CMSG) ---

void WorldSession::HandleAuthSession(WorldPacket &packet) {
  uint8 digest[20];
  std::vector<uint8> localChallenge(4);
  uint16 build;
  uint32 realmId;
  int32 loginServerId;

  // 1. Read seeds, digest and build using the Scattered format
  HandleAuthSessionScattered(packet, digest, localChallenge, build, realmId,
                             loginServerId);

  // 2. Read addon data size and skip it
  uint32 addonDataSize = 0;
  if (packet.GetReadPos() + 4 <= packet.Size()) {
    addonDataSize = packet.Read<uint32>();
  }
  if (addonDataSize > 0 &&
      (packet.GetReadPos() + addonDataSize) <= packet.Size()) {
    packet.SetReadPos(packet.GetReadPos() + addonDataSize);
  }

  // 3. Extract Account Name using BitReader (Cataclysm 4.3.4 Build 15595)
  BitReader br(packet);
  br.ReadBit(); // UseIPv6
  uint32 accountNameLength = br.ReadBits(12);
  std::string account = br.ReadString(accountNameLength);

  LOG_INFO("CMSG_AUTH_SESSION: Account: '{}', Build: {}, RealmID: {}, "
           "ClientSeed: {}, Packet Size: {}",
           account, build, realmId,
           Crypto::ToHexString(localChallenge.data(), 4), packet.Size());

  auto accountOpt = _authService->FindAccount(account);
  if (!accountOpt) {
    LOG_ERROR("CMSG_AUTH_SESSION: Account '{}' not found.", account);
    Close();
    return;
  }

  std::vector<uint8_t> K = _authService->GetSessionKey(accountOpt->id);
  if (K.empty()) {
    LOG_ERROR("CMSG_AUTH_SESSION: No session key K for account '{}'.", account);
    Close();
    return;
  }

  // Initialize WorldCrypt IMMEDIATELY after getting K (Cataclysm requirement)
  _crypt.Init(K);
  LOG_INFO("[AUTH] WorldCrypt initialized with 40 bytes of K");

  // 4. Perform Digest validation
  // SHA1(Account, t(0), ClientChallenge, ServerSeed, SessionKey)
  Crypto::SHA1 sha;
  sha.Update(Crypto::ToUpper(account));

  uint32 t = 0;
  sha.Update(t);

  sha.Update(localChallenge.data(), 4);
  sha.Update(_serverSeed);
  sha.Update(K.data(), K.size());

  Crypto::SHA1Hash calculatedDigest = sha.Finalize();

  if (std::memcmp(calculatedDigest.data(), digest, 20) != 0) {
    LOG_ERROR("CMSG_AUTH_SESSION: Digest validation failed for account '{}'!",
              account);
    LOG_INFO("Calculated: {}", Crypto::ToHexString(calculatedDigest));
    LOG_INFO("Received:   {}", Crypto::ToHexString(digest, 20));
    Close();
    return;
  }

  _accountId = accountOpt->id;
  LOG_INFO("CMSG_AUTH_SESSION: Digest validated successfully for account '{}'.",
           account);

  SendAuthResponse();
  SendAddonInfo();
  // Reference parity: after auth success, send cache version and tutorial flags.
  // FirelandsCore does: SendAddonsInfo(); SendClientCacheVersion(...); SendTutorialsData();
  SendClientCacheVersion(0);
  SendTutorialFlags();
}

void WorldSession::HandleAuthSessionScattered(
    WorldPacket &packet, uint8 *digest, std::vector<uint8> &localChallenge,
    uint16 &build, uint32 &realmId, int32 &loginServerId) {
  loginServerId = packet.Read<int32>();
  uint32 battlegroupId = packet.Read<uint32>();
  int8 loginServerType = packet.Read<int8>();

  digest[10] = packet.Read<uint8>();
  digest[18] = packet.Read<uint8>();
  digest[12] = packet.Read<uint8>();
  digest[5] = packet.Read<uint8>();

  uint64 dosResponse = packet.Read<uint64>();

  digest[15] = packet.Read<uint8>();
  digest[9] = packet.Read<uint8>();
  digest[19] = packet.Read<uint8>();
  digest[4] = packet.Read<uint8>();
  digest[7] = packet.Read<uint8>();
  digest[16] = packet.Read<uint8>();
  digest[3] = packet.Read<uint8>();

  build = packet.Read<uint16>();
  digest[8] = packet.Read<uint8>();

  realmId = packet.Read<uint32>();
  int8 buildType = packet.Read<int8>();

  digest[17] = packet.Read<uint8>();
  digest[6] = packet.Read<uint8>();
  digest[0] = packet.Read<uint8>();
  digest[1] = packet.Read<uint8>();
  digest[11] = packet.Read<uint8>();

  localChallenge[0] = packet.Read<uint8>();
  localChallenge[1] = packet.Read<uint8>();
  localChallenge[2] = packet.Read<uint8>();
  localChallenge[3] = packet.Read<uint8>();

  digest[2] = packet.Read<uint8>();
  uint32 regionId = packet.Read<uint32>();

  digest[14] = packet.Read<uint8>();
  digest[13] = packet.Read<uint8>();
}

void WorldSession::HandleAuthSessionStandard(WorldPacket &packet, uint16 &build,
                                             uint8 *digest,
                                             std::vector<uint8> &localChallenge,
                                             uint32 &realmId) {
  build = static_cast<uint16>(packet.Read<uint32>());
  packet.Read<uint32>(); // loginServerId or unknown
  realmId = packet.Read<uint32>();

  packet.Read(localChallenge.data(), 4);
  packet.Read<uint32>(); // Seed
  packet.Read<uint32>(); // Unk
  packet.Read<uint32>(); // Unk

  packet.Read(digest, 20);
}

void WorldSession::SendAuthResponse() {
  WorldPacket response(SMSG_AUTH_RESPONSE);
  BitWriter bw(response);
  bw.WriteBit(false); // hasWaitInfo
  bw.WriteBit(true);  // hasSuccessInfo
  bw.Flush();

  response.Append<uint32>(0); // TimeRemain
  response.Append<uint8>(3);  // ActiveExpansionLevel (Cata)
  response.Append<uint32>(0); // TimeSecondsUntilPCKick
  response.Append<uint8>(3);  // AccountExpansionLevel (Cata)
  response.Append<uint32>(0); // TimeRested
  response.Append<uint8>(0);  // TimeOptions
  response.Append<uint8>(12); // Result (AUTH_OK = 12)

  SendPacket(response);
  LOG_INFO("[AUTH] SMSG_AUTH_RESPONSE (AUTH_OK) sent.");
}

void WorldSession::SendAddonInfo() {
  WorldPacket data(SMSG_ADDON_INFO);
  data.Append<uint32>(0); // Banned addon count (0)

  SendPacket(data);
  LOG_INFO("[AUTH] SMSG_ADDON_INFO sent (15595 parity).");
}

void WorldSession::HandleCharEnum(WorldPacket & /*packet*/) {
  auto characters = _charService->GetCharactersForAccount(_accountId);
  uint32 count = static_cast<uint32>(characters.size());

  LOG_INFO("CMSG_CHAR_ENUM: Found {} characters for account {}", count,
           _accountId);

  WorldPacket response(SMSG_CHAR_ENUM);
  BitWriter bw(response);

  // Cata 4.3.4 (15595) Bit Header:
  bw.WriteBits(0, 23); // FactionChangeRestrictions size
  bw.WriteBit(true);   // Success
  bw.WriteBits(count, 17);

  if (count == 0) {
    bw.Flush();
    SendPacket(response);
    return;
  }

  struct GuidData {
    uint8 g[8];
    uint8 gg[8];
  };
  std::vector<GuidData> gd_list(count);

  for (uint32 i = 0; i < count; ++i) {
    uint64 guid = characters[i]->GetGuid();
    uint64 guildGuid = 0; // Guilds not implemented

    for (int b = 0; b < 8; ++b) {
      gd_list[i].g[b] = (guid >> (b * 8)) & 0xFF;
      gd_list[i].gg[b] = (guildGuid >> (b * 8)) & 0xFF;
    }

    auto &gd = gd_list[i];
    // Exact bit order for 15595 Character Enum
    bw.WriteBit(gd.g[3]);
    bw.WriteBit(gd.gg[1]);
    bw.WriteBit(gd.gg[7]);
    bw.WriteBit(gd.gg[2]);
    bw.WriteBits(characters[i]->GetName().length(), 7);
    bw.WriteBit(gd.g[4]);
    bw.WriteBit(gd.g[7]);
    bw.WriteBit(gd.gg[3]);
    bw.WriteBit(gd.g[5]);
    bw.WriteBit(gd.gg[6]);
    bw.WriteBit(gd.g[1]);
    bw.WriteBit(gd.gg[5]);
    bw.WriteBit(gd.gg[4]);
    bw.WriteBit(characters[i]->IsFirstLogin());
    bw.WriteBit(gd.g[0]);
    bw.WriteBit(gd.g[2]);
    bw.WriteBit(gd.g[6]);
    bw.WriteBit(gd.gg[0]);
  }
  bw.Flush();

  for (uint32 i = 0; i < count; ++i) {
    auto const &ch = characters[i];
    auto &gd = gd_list[i];

    // Exact byte order for 15595 Character Enum
    response.Append<uint8>(ch->GetClass());

    // Equipment (VisualItems) - 23 slots in Cata
    for (int slot = 0; slot < 23; ++slot) {
      response.Append<uint8>(0);  // InvType
      response.Append<uint32>(0); // DisplayID
      response.Append<uint32>(0); // DisplayEnchantID
    }

    response.Append<uint32>(0); // PetCreatureFamilyID
    response.WriteByteSeq(gd.gg[2]);
    response.Append<uint8>(i); // ListPosition
    response.Append<uint8>(ch->GetHairStyle());
    response.WriteByteSeq(gd.gg[3]);
    response.Append<uint32>(0); // PetCreatureDisplayID
    response.Append<uint32>(ch->GetCharacterFlags());
    response.Append<uint8>(ch->GetHairColor());
    response.WriteByteSeq(gd.g[4]);
    response.Append<int32>(ch->GetMapId());
    response.WriteByteSeq(gd.gg[5]);
    response.Append<float>(ch->GetZ());
    response.WriteByteSeq(gd.gg[6]);
    response.Append<uint32>(0); // PetExperienceLevel
    response.WriteByteSeq(gd.g[3]);
    response.Append<float>(ch->GetY());
    response.Append<uint32>(ch->GetCustomizationFlags());
    response.Append<uint8>(ch->GetFacialHair());
    response.WriteByteSeq(gd.g[7]);
    response.Append<uint8>(ch->GetGender());
    response.WriteStringNoNull(ch->GetName()); // Length is already in bitstream
    response.Append<uint8>(ch->GetFace());
    response.WriteByteSeq(gd.g[0]);
    response.WriteByteSeq(gd.g[2]);
    response.WriteByteSeq(gd.gg[1]);
    response.WriteByteSeq(gd.gg[7]);
    response.Append<float>(ch->GetX());
    response.Append<uint8>(ch->GetSkin());
    response.Append<uint8>(ch->GetRace());
    response.Append<uint8>(ch->GetLevel());
    response.WriteByteSeq(gd.g[6]);
    response.WriteByteSeq(gd.gg[4]);
    response.WriteByteSeq(gd.gg[0]);
    response.WriteByteSeq(gd.g[5]);
    response.WriteByteSeq(gd.g[1]);
    response.Append<int32>(ch->GetZoneId());
  }

  SendPacket(response);
  LOG_INFO("SMSG_CHAR_ENUM sent.");
}

void WorldSession::HandleCharCreate(WorldPacket &packet) {
  std::string name = packet.ReadString();
  uint8 race = packet.Read<uint8>();
  uint8 klass = packet.Read<uint8>();
  uint8 gender = packet.Read<uint8>();
  uint8 skin = packet.Read<uint8>();
  uint8 face = packet.Read<uint8>();
  uint8 hairStyle = packet.Read<uint8>();
  uint8 hairColor = packet.Read<uint8>();
  uint8 facialHair = packet.Read<uint8>();
  uint8 outfitId = packet.Read<uint8>();

  LOG_INFO("CMSG_CHAR_CREATE: Name='{}', Race={}, Class={}", name, race, klass);

  bool success =
      _charService->CreateCharacter(_accountId, name, race, klass, gender, skin,
                                    face, hairStyle, hairColor, facialHair);

  WorldPacket response(SMSG_CHAR_CREATE);
  response.Append<uint8>(success ? 0x2F : 0x30);
  SendPacket(response);
}

void WorldSession::HandleCharDelete(WorldPacket &packet) {
  uint64 guid = packet.Read<uint64>();
  LOG_INFO("CMSG_CHAR_DELETE for GUID: {}", guid);

  bool success =
      _charService->DeleteCharacter(static_cast<uint32>(guid), _accountId);

  WorldPacket response(SMSG_CHAR_DELETE);
  response.Append<uint8>(success ? 0x47 : 0x48);
  SendPacket(response);
}

void WorldSession::HandlePlayerLogin(WorldPacket &packet) {
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

  uint64 guid = 0;
  std::memcpy(&guid, guid_bytes, 8);

  LOG_INFO("CMSG_PLAYER_LOGIN for GUID: {}", guid);
  _playerGuid = guid;
  _timeSyncNextCounter = 0;

  auto characterOpt = _charService->GetCharacterByGuid(guid);
  if (!characterOpt) {
    LOG_ERROR("CMSG_PLAYER_LOGIN: Character not found.");
    Close();
    return;
  }
  const auto &character = *characterOpt;

  // Login SMSG order aligned with firelands-cata-ref CharacterHandler::HandlePlayerLogin
  // and Player::SendInitialPacketsBeforeAddToMap (see Player.cpp ~23604).
  SendDungeonDifficulty(false);

  SendAccountDataTimes(0x15);
  SendLearnedDanceMoves();
  SendHotfixNotifyBlobEmpty();

  // Player::SendInitialPacketsBeforeAddToMap (same relative order as reference)
  SendClientControlUpdate(guid);
  SendBindPointUpdate();
  SendWorldServerInfo();
  SendSetProficiency(1, 0xFFFFFFFF);
  SendSetProficiency(2, 0xFFFFFFFF);
  SendKnownSpells(character.IsFirstLogin(), BuildDefaultKnownSpells(character.GetClass()));
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

  // Reference sends MOTD and SMSG_FEATURE_SYSTEM_STATUS after BeforeAddToMap.
  SendMotd();
  SendFeatureSystemStatus();
  SendTutorialFlags();
  SendClientCacheVersion(0);

  // Create in-memory player and add to map BEFORE sending verify-world + worldstates
  _mapId = character.GetMapId();
  MovementInfo move;
  static auto startTime = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  move.time = static_cast<uint32>(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
  move.x = character.GetX();
  move.y = character.GetY();
  move.z = character.GetZ();
  move.orientation = character.GetOrientation();

  auto player = std::make_shared<Player>(guid, shared_from_this());
  player->SetPosition(move);
  WorldService::Instance().AddPlayerToMap(_mapId, player);

  SendLoginVerifyWorld(character.GetMapId(), character.GetX(), character.GetY(),
                       character.GetZ(), character.GetOrientation());

  // Now that the player is on the map, send create/update data and "after add" packets
  UpdateData update(character.GetMapId());

  std::map<uint16, uint32> fields;
  fields[OBJECT_FIELD_GUID] = (uint32)(guid & 0xFFFFFFFF);
  fields[OBJECT_FIELD_GUID + 1] = (uint32)(guid >> 32);
  fields[OBJECT_FIELD_TYPE] = (1 << TYPEID_OBJECT) | (1 << TYPEID_UNIT) | (1 << TYPEID_PLAYER);
  fields[OBJECT_FIELD_SCALE_X] = 0x3F800000;

  uint8 bytes0[4] = {character.GetRace(), character.GetClass(), character.GetGender(), 0};
  std::memcpy(&fields[UNIT_FIELD_BYTES_0], bytes0, 4);

  fields[UNIT_FIELD_HEALTH] = character.GetHealth();
  fields[UNIT_FIELD_MAXHEALTH] = character.GetMaxHealth();
  // Power1 (mana). Warriors/rogues/DKs use 0 mana; safe to send 0 for all.
  fields[UNIT_FIELD_POWER1] = 0;
  fields[UNIT_FIELD_MAXPOWER1] = 0;
  fields[UNIT_FIELD_LEVEL] = character.GetLevel();
  fields[UNIT_FIELD_FACTIONTEMPLATE] = character.GetFactionTemplate();
  fields[UNIT_FIELD_DISPLAYID] = character.GetDisplayId();
  // NativeDisplayId must equal DisplayId on creation; missing field causes a
  // client crash when transform auras are processed at the end of loading.
  fields[UNIT_FIELD_NATIVEDISPLAYID] = character.GetDisplayId();
  // BYTES_2: PvP flags byte — must be present (0 = normal, no PvP flags).
  fields[UNIT_FIELD_BYTES_2] = 0;

  update.AddCreateObject(guid, TYPEID_PLAYER, move, fields);

  WorldPacket updatePacket(SMSG_UPDATE_OBJECT);
  update.Build(updatePacket);
  SendPacket(updatePacket);

  // Equivalent of Player::SendInitialPacketsAfterAddToMap: world states, then
  // ResetTimeSync + SendTimeSync (WorldSession.cpp in reference).
  SendInitWorldStates(character.GetMapId(), character.GetZoneId());
  WorldPacket timeSync(SMSG_TIME_SYNC_REQ);
  timeSync.Append<uint32>(_timeSyncNextCounter++);
  SendPacket(timeSync);
  SendLoadCUFProfiles();

  LOG_INFO("Player {} logged in and spawned at Map {}", guid, _mapId);

  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireEvent("player_login", guid);
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
    response.Append<uint8>(RESPONSE_FAILURE);
    SendPacket(response);
    return;
  }

  response.Append<uint8>(RESPONSE_SUCCESS);
  std::string const realmName =
      Config::Instance().GetNested<std::string>({"World", "RealmName"}, "");
  AppendPlayerGuidLookupData(response, *chOpt, realmName);
  SendPacket(response);
}

void WorldSession::HandleQueryTime(WorldPacket & /*packet*/) {
  // CMSG_QUERY_TIME: client asks server time for day/night and reset timers.
  WorldPacket response(SMSG_QUERY_TIME_RESPONSE);
  response.Append<uint32>(static_cast<uint32>(std::time(nullptr)));
  response.Append<uint32>(0); // next daily reset (unknown/not implemented)
  SendPacket(response);
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

void WorldSession::HandleTimeSyncResp(WorldPacket &packet) {
  uint32 counter = packet.Read<uint32>();
  uint32 clientTime = packet.Read<uint32>();
  LOG_DEBUG("CMSG_TIME_SYNC_RESP: Counter: {}, ClientTime: {}", counter,
            clientTime);

  // Keep issuing time sync samples while in world (client expects ongoing
  // SMSG_TIME_SYNC_REQ after login; see ref WorldSession::ReadMovementInfo flow).
  if (_playerGuid != 0) {
    WorldPacket next(SMSG_TIME_SYNC_REQ);
    next.Append<uint32>(_timeSyncNextCounter++);
    SendPacket(next);
  }
}

void WorldSession::HandleMoveTimeSkipped(WorldPacket &packet) {
  uint32 time = packet.Read<uint32>();
  BitReader br(packet);
  for (int i = 0; i < 8; ++i) { if (br.ReadBit()) packet.Read<uint8>(); }
  LOG_INFO("CMSG_MOVE_TIME_SKIPPED: Time: {}", time);
}

void WorldSession::HandleMessageChat(WorldPacket &packet) {
  uint32 type = packet.Read<uint32>();
  uint32 language = packet.Read<uint32>();
  if (type == CHAT_MSG_WHISPER || type == CHAT_MSG_CHANNEL) packet.ReadString();
  std::string message = packet.ReadString();

  if (_commandService->IsCommand(message)) {
    _commandService->ExecuteCommand(shared_from_this(), message);
    return;
  }

  WorldPacket response(SMSG_MESSAGECHAT);
  response.Append<uint8>(static_cast<uint8>(type));
  response.Append<uint32>(language);
  response.Append<uint64>(_playerGuid);
  response.Append<uint32>(0);
  response.Append<uint64>(_playerGuid);
  response.Append<uint32>(static_cast<uint32>(message.length() + 1));
  response.WriteString(message);
  response.Append<uint8>(0);
  SendPacket(response);

  if (_playerGuid != 0 &&
      (type == CHAT_MSG_SAY || type == CHAT_MSG_YELL)) {
    if (auto map = WorldService::Instance().GetMap(_mapId)) {
      map->BroadcastPacketToNearby(_playerGuid, response, false);
    }
  }
}

void WorldSession::HandleGossipHello(WorldPacket &packet) {
  const uint64 npcGuid = ReadClientTargetGuid(packet);
  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireGossipHello(npcGuid);
  }
}

void WorldSession::HandleGossipSelectOption(WorldPacket &packet) {
  const uint64 npcGuid = ReadClientTargetGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32) * 2 > packet.Size()) {
    return;
  }
  const uint32 menuId = packet.Read<uint32>();
  const uint32 listId = packet.Read<uint32>();
  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireGossipSelect(npcGuid, menuId, listId);
  }
}

void WorldSession::HandleRealmSplit(WorldPacket &packet) {
  uint32 unk = packet.Read<uint32>();
  WorldPacket data(SMSG_REALM_SPLIT);
  data.Append<uint32>(unk);
  data.Append<uint32>(0);
  data.WriteString("01/01/01");
  SendPacket(data);
}

void WorldSession::HandleReadyForAccountDataTimes(WorldPacket & /*packet*/) {
  SendAccountDataTimes(0x15);
}

void WorldSession::HandleUpdateAccountData(WorldPacket &packet) {
  // Reference behavior: always ACK with SMSG_UPDATE_ACCOUNT_DATA_COMPLETE.
  // We don't persist the data yet, but the client expects the completion packet.
  uint32 type = packet.Read<uint32>();
  uint32 timestamp = packet.Read<uint32>();
  uint32 decompressedSize = packet.Read<uint32>();

  (void)timestamp;

  // In reference, NUM_ACCOUNT_DATA_TYPES bounds are enforced. We keep it permissive for now,
  // but still ACK to avoid client-side crashes.
  if (decompressedSize > 0) {
    // Skip remaining compressed payload (if any)
    while (packet.GetReadPos() < packet.Size()) {
      packet.Read<uint8>();
    }
  }

  WorldPacket ack(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE);
  ack.Append<uint32>(type);
  ack.Append<uint32>(0); // result (0 = OK)
  SendPacket(ack);
}

void WorldSession::HandleMovement(WorldPacket &packet) {
  MovementInfo move;
  ReadMovementInfo(packet, move);
  _position = move;

  if (IsClientMovementOpcode(static_cast<WorldOpcode>(packet.GetOpcode()))) {
    auto map = WorldService::Instance().GetMap(_mapId);
    if (map) {
      map->UpdateObjectPosition(_playerGuid, move);
      WorldPacket broadcast(packet.GetOpcode(), packet.Size());
      broadcast.Append(packet.GetBuffer(), packet.Size());
      map->BroadcastPacketToNearby(_playerGuid, broadcast);
      SendPacket(broadcast);
    }
  }
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

void WorldSession::SendAccountDataTimes(uint32 mask) {
  WorldPacket data(SMSG_ACCOUNT_DATA_TIMES);
  data.Append<uint32>(static_cast<uint32>(std::time(nullptr)));
  data.Append<uint8>(1);
  data.Append<uint32>(mask);
  for (int i = 0; i < 8; ++i) { if (mask & (1 << i)) data.Append<uint32>(0); }
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

// --- Helpers ---

void WorldSession::ReadMovementInfo(WorldPacket &packet, MovementInfo &move) {
  if (packet.Size() >= 26) {
    move.flags = packet.Read<uint32>();
    move.flags2 = packet.Read<uint16>();
    move.time = packet.Read<uint32>();
    move.x = packet.Read<float>();
    move.y = packet.Read<float>();
    move.z = packet.Read<float>();
    move.orientation = packet.Read<float>();
  }
}

void WorldSession::SendNotification(const std::string &message) {
  WorldPacket response(SMSG_MESSAGECHAT);
  response.Append<uint8>(CHAT_MSG_SYSTEM);
  response.Append<uint32>(LANG_UNIVERSAL);
  response.Append<uint64>(0);
  response.Append<uint32>(0);
  response.Append<uint64>(0);
  response.Append<uint32>(static_cast<uint32>(message.length() + 1));
  response.WriteString(message);
  response.Append<uint8>(0);
  SendPacket(response);
}

void WorldSession::TeleportTo(uint32 /*mapId*/, float x, float y, float z, float orientation) {
  _position.x = x;
  _position.y = y;
  _position.z = z;
  _position.orientation = orientation;
  SendNotification("Teleported to: " + std::to_string(x) + ", " +
                   std::to_string(y) + ", " + std::to_string(z));
}

} // namespace Firelands
