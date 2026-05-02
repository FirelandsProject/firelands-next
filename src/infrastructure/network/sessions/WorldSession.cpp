#include <application/ports/IMapNotifier.h>
#include <application/services/WorldService.h>
#include <domain/models/Character.h>
#include <domain/world/Player.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionMovementChecks.h>
#include <infrastructure/persistence/MySqlAccountDataRepository.h>
#include <shared/Config.h>
#include <shared/Logger.h>
#include <shared/network/UpdateData.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/packets/MotdPacket.h>
#include <shared/network/packets/VerifyWorldPacket.h>
#include <shared/network/packets/SetProficiencyPacket.h>
#include <shared/network/BitReader.h>
#include <shared/network/SpellCastWire.h>
#include <shared/game/ChatLanguages.h>
#include <shared/game/EquipmentCache.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/WowGuid.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <map>
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
    // Do not use post-4.3.4 spell ids (e.g. 86475): some clients stop applying
    // `SMSG_SEND_KNOWN_SPELLS` after an unknown id, which blocks `/say` (no
    // `CMSG_MESSAGECHAT_SAY`) until language passives never register.
    return {585, 589, 2061, 17, 139, 2050, 8092};
  case 6: // Death Knight
    return {48263, 45524, 49998, 47528, 48721, 45529, 48792};
  case 7: // Shaman
    return {331, 8042, 8017, 8050, 324, 51730, 5185, 52127};
  case 8: // Mage
    return {116, 133, 2136, 1459, 130, 1953, 118};
  case 9: // Warlock
    return {172, 348, 687, 1454, 5782, 980, 603};
  case 11: // Druid
    return {8921, 5185, 774, 768, 1126, 339, 467};
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

std::map<uint16, uint32> BuildItemCreateFields(uint64 itemObjectGuid,
                                               uint64 ownerGuid,
                                               uint32 itemEntry,
                                               uint32 stackCount) {
  std::map<uint16, uint32> fields;
  for (uint16_t i = 0; i < ITEM_END; ++i)
    fields[i] = 0;

  uint32 igLo = 0;
  uint32 igHi = 0;
  uint32 owLo = 0;
  uint32 owHi = 0;
  WriteGuidToTwoUint32(itemObjectGuid, igLo, igHi);
  WriteGuidToTwoUint32(ownerGuid, owLo, owHi);

  fields[OBJECT_FIELD_GUID] = igLo;
  fields[OBJECT_FIELD_GUID + 1] = igHi;
  fields[OBJECT_FIELD_DATA] = 0;
  fields[OBJECT_FIELD_DATA + 1] = 0;
  fields[OBJECT_FIELD_TYPE] = kTypeMaskItem;
  fields[OBJECT_FIELD_ENTRY] = itemEntry;
  fields[OBJECT_FIELD_SCALE_X] = 0x3F800000;
  fields[OBJECT_FIELD_PADDING] = 0;

  fields[ITEM_FIELD_OWNER] = owLo;
  fields[ITEM_FIELD_OWNER + 1] = owHi;
  fields[ITEM_FIELD_CONTAINED] = owLo;
  fields[ITEM_FIELD_CONTAINED + 1] = owHi;
  fields[ITEM_FIELD_STACK_COUNT] = stackCount;
  return fields;
}

/// Partial player VALUES refresh (bag 0 equip + main backpack grid) after moves.
std::map<uint16, uint32> BuildPlayerBag0InventoryValues(Character const &character) {
  std::map<uint16, uint32> fields;
  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32 const entry = character.GetVisibleItemEntry(slot);
    uint32 const itemGuidLow = character.GetVisibleItemGuidLow(slot);
    uint16 const base = static_cast<uint16>(
        PLAYER_VISIBLE_ITEM_1_ENTRYID + static_cast<uint16>(slot * 2));
    fields[base] = entry;
    fields[static_cast<uint16>(base + 1)] = 0;

    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const invBase = static_cast<uint16>(
        PLAYER_FIELD_INV_SLOT_HEAD + static_cast<uint16>(slot * 2));
    fields[invBase] = ilo;
    fields[static_cast<uint16>(invBase + 1)] = ihi;
  }
  for (size_t packIndex = 0; packIndex < kPackSlotCount; ++packIndex) {
    uint32 const itemGuidLow = character.GetPackItemGuidLow(packIndex);
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const packBase = static_cast<uint16>(
        PLAYER_FIELD_PACK_SLOT_1 + static_cast<uint16>(packIndex * 2));
    fields[packBase] = ilo;
    fields[static_cast<uint16>(packBase + 1)] = ihi;
  }
  return fields;
}

void SetPackedShort(std::map<uint16, uint32> &fields, uint16 field,
                    uint8 slot, uint16 value) {
  uint32 &packed = fields[static_cast<uint16>(field + (slot / 2))];
  if ((slot % 2) == 0)
    packed = (packed & 0xFFFF0000u) | value;
  else
    packed = (packed & 0x0000FFFFu) | (static_cast<uint32>(value) << 16);
}

void AddLanguageSkillFields(std::map<uint16, uint32> &fields, uint8 race) {
  std::vector<uint32> skills;
  AppendRacialLanguageSkills(race, skills);
  constexpr uint16 kRank = 300;
  for (size_t i = 0; i < skills.size() && i < 64; ++i) {
    uint8 const slot = static_cast<uint8>(i);
    uint16 const skill = static_cast<uint16>(skills[i]);
    SetPackedShort(fields, PLAYER_SKILL_LINEID_0, slot, skill);
    SetPackedShort(fields, PLAYER_SKILL_STEP_0, slot, 0);
    SetPackedShort(fields, PLAYER_SKILL_RANK_0, slot, kRank);
    SetPackedShort(fields, PLAYER_SKILL_MAX_RANK_0, slot, kRank);
    SetPackedShort(fields, PLAYER_SKILL_MODIFIER_0, slot, 0);
    SetPackedShort(fields, PLAYER_SKILL_TALENT_0, slot, 0);
  }
}

std::map<uint16, uint32> BuildPlayerUpdateFields(uint64 guid,
                                                 Character const &character) {
  std::map<uint16, uint32> fields;
  fields[OBJECT_FIELD_GUID] = (uint32)(guid & 0xFFFFFFFF);
  fields[OBJECT_FIELD_GUID + 1] = (uint32)(guid >> 32);
  fields[OBJECT_FIELD_TYPE] =
      (1 << TYPEID_OBJECT) | (1 << TYPEID_UNIT) | (1 << TYPEID_PLAYER);
  fields[OBJECT_FIELD_SCALE_X] = 0x3F800000;

  uint8 bytes0[4] = {character.GetRace(), character.GetClass(),
                     character.GetGender(), 0};
  std::memcpy(&fields[UNIT_FIELD_BYTES_0], bytes0, 4);

  fields[UNIT_FIELD_HEALTH] = character.GetHealth();
  fields[UNIT_FIELD_MAXHEALTH] = character.GetMaxHealth();
  fields[UNIT_FIELD_POWER1] = 0;
  fields[UNIT_FIELD_MAXPOWER1] = 0;
  fields[UNIT_FIELD_LEVEL] = character.GetLevel();
  fields[UNIT_FIELD_FACTIONTEMPLATE] = character.GetFactionTemplate();
  fields[UNIT_FIELD_DISPLAYID] = character.GetDisplayId();
  fields[UNIT_FIELD_NATIVEDISPLAYID] = character.GetDisplayId();
  fields[UNIT_FIELD_BYTES_2] = 0;

  AddLanguageSkillFields(fields, character.GetRace());

  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32 const entry = character.GetVisibleItemEntry(slot);
    uint32 const itemGuidLow = character.GetVisibleItemGuidLow(slot);
    uint16 const base = static_cast<uint16>(
        PLAYER_VISIBLE_ITEM_1_ENTRYID + static_cast<uint16>(slot * 2));
    fields[base] = entry;
    fields[static_cast<uint16>(base + 1)] = 0;

    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const invBase = static_cast<uint16>(
        PLAYER_FIELD_INV_SLOT_HEAD + static_cast<uint16>(slot * 2));
    fields[invBase] = ilo;
    fields[static_cast<uint16>(invBase + 1)] = ihi;
  }
  for (size_t packIndex = 0; packIndex < kPackSlotCount; ++packIndex) {
    uint32 const itemGuidLow = character.GetPackItemGuidLow(packIndex);
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    uint32 ilo = 0;
    uint32 ihi = 0;
    WriteGuidToTwoUint32(itemOg, ilo, ihi);
    uint16 const packBase = static_cast<uint16>(
        PLAYER_FIELD_PACK_SLOT_1 + static_cast<uint16>(packIndex * 2));
    fields[packBase] = ilo;
    fields[static_cast<uint16>(packBase + 1)] = ihi;
  }
  return fields;
}

void SendPlayerCreateToNotifier(std::shared_ptr<IMapNotifier> target,
                                uint32 mapId, uint64 objectGuid,
                                Character const &character,
                                MovementInfo const &move) {
  if (!target)
    return;
  UpdateData update(mapId);
  update.AddCreateObject(objectGuid, TYPEID_PLAYER, move,
                         BuildPlayerUpdateFields(objectGuid, character));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  target->SendPacket(pkt);
}

} // namespace

WorldSession::WorldSession(
    tcp::socket socket, std::shared_ptr<AuthService> authService,
    std::shared_ptr<CharacterService> charService,
    std::shared_ptr<ICommandService> commandService,
    std::shared_ptr<MySqlAccountDataRepository> accountDataRepo,
    std::shared_ptr<LanguagesDbc const> languagesDbc,
    std::shared_ptr<SpellDbc const> spellDbc)
    : _socket(std::move(socket)), _authService(std::move(authService)),
      _charService(std::move(charService)),
      _commandService(std::move(commandService)),
      _accountDataRepo(std::move(accountDataRepo)),
      _languagesDbc(std::move(languagesDbc)),
      _spellDbc(std::move(spellDbc)), _serverSeed(0),
      _accountId(0), _timeSyncPeriodicTimer(_socket.get_executor()) {}

WorldSession::~WorldSession() = default;

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

  LOG_INFO("[SMSG] {} payload={} wire={}",
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
  LOG_INFO("[SEND] raw {} bytes (handshake / non-opcode)", shared_buffer->size());
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
void WorldSession::Close() {
  CancelPeriodicTimeSync();
  if (_socket.is_open()) {
    LOG_INFO("Closing WorldSession for {}", GetIpAddress());
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

// --- Client Packet Handlers (CMSG) ---

void WorldSession::HandleCharEnum(WorldPacket & /*packet*/) {
  auto characters = _charService->GetCharactersForAccount(_accountId);
  uint32 count = static_cast<uint32>(characters.size());

  LOG_DEBUG("CMSG_CHAR_ENUM: Found {} characters for account {}", count,
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

    const auto visualItems = EquipmentCache::Parse(ch->GetEquipmentCache());
    // Equipment (VisualItems) - 23 slots in Cata
    for (int slot = 0; slot < 23; ++slot) {
      auto const &visualSlot = visualItems[slot];
      response.Append<uint8>(visualSlot.invType);
      response.Append<uint32>(visualSlot.displayId);
      response.Append<uint32>(visualSlot.displayEnchantId);
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

  LOG_DEBUG("CMSG_CHAR_CREATE: Name='{}', Race={}, Class={}", name, race, klass);

  bool success =
      _charService->CreateCharacter(_accountId, name, race, klass, gender, skin,
                                    face, hairStyle, hairColor, facialHair,
                                    outfitId);

  WorldPacket response(SMSG_CHAR_CREATE);
  response.Append<uint8>(success ? 0x2F : 0x30);
  SendPacket(response);
}

void WorldSession::HandleCharDelete(WorldPacket &packet) {
  uint64 guid = packet.Read<uint64>();
  LOG_DEBUG("CMSG_CHAR_DELETE for GUID: {}", guid);

  bool success =
      _charService->DeleteCharacter(static_cast<uint32>(guid), _accountId);

  WorldPacket response(SMSG_CHAR_DELETE);
  response.Append<uint8>(success ? 0x47 : 0x48);
  SendPacket(response);
}

void WorldSession::HandlePlayerLogin(WorldPacket &packet) {
  uint64 guid = 0;
  LoginReadPackedPlayerGuid(packet, guid);

  LOG_DEBUG("CMSG_PLAYER_LOGIN for GUID: {}", guid);
  _playerGuid = guid;
  _timeSyncNextCounter = 0;

  auto characterOpt = _charService->GetCharacterByGuid(guid);
  if (!characterOpt) {
    LOG_ERROR("CMSG_PLAYER_LOGIN: Character not found.");
    Close();
    return;
  }
  Character const &character = *characterOpt;
  _playerRace = character.GetRace();

  LoginSendAccountDataAndPreMapPackets(guid, character);
  LoginBuildKnownSpellsAndSendSpellbook(character);
  LoginSendMotdAndMetaPackets();

  MovementInfo move{};
  LoginResolveMapPosition(guid, character, move);

  LoginSpawnInWorld(guid, move);
  LoginSendCreateUpdatesAndMutualVisibility(guid, character, move);
  LoginFinalizeWorldEntry(guid);
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
      for (uint32 sid : BuildDefaultKnownSpells(character.GetClass()))
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
    if (_spellDbc && _spellDbc->IsLoaded()) {
      for (auto it = spells.begin(); it != spells.end();) {
        uint32 const sid = *it;
        // Never drop passive "Language *" spells: wrong-era DBC would break /say.
        if (!IsLanguagePassiveSpell(sid) && !_spellDbc->HasSpell(sid)) {
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
    EnsureRacialLanguageSpells(static_cast<uint8>(character.GetRace()), spells);
    PrioritizeDefaultLanguageSpell(static_cast<uint8>(character.GetRace()),
                                   spells);
    _knownSpells = std::move(spells);
    uint32 const defaultLang = DefaultLanguageForRace(character.GetRace());
    uint32 const defaultLangSpell = LanguageSpellIdForLang(defaultLang);
    {
      std::string ids;
      for (size_t i = 0; i < _knownSpells.size(); ++i) {
        if (i)
          ids.push_back(',');
        ids += std::to_string(_knownSpells[i]);
      }
      LOG_INFO("[CHAT] login race={} class={} defaultLang={} langSpell={} known={} "
               "spells={} ids=[{}]",
               static_cast<uint32>(character.GetRace()),
               static_cast<uint32>(character.GetClass()), defaultLang,
               defaultLangSpell,
               PlayerKnowsLanguage(_knownSpells, defaultLang) ? 1 : 0,
               _knownSpells.size(), ids);
    }
  }
  _gcdReady = {};
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

void WorldSession::LoginSpawnInWorld(uint64 guid, MovementInfo const &move) {
  auto player = std::make_shared<Player>(guid, shared_from_this());
  player->SetPosition(move);
  WorldService::Instance().AddPlayerToMap(_mapId, player);

  SendLoginVerifyWorld(_mapId, move.x, move.y, move.z, move.orientation);
}

void WorldSession::LoginSendCreateUpdatesAndMutualVisibility(
    uint64 guid, Character const &character, MovementInfo const &move) {
  // Now that the player is on the map, send create/update data and "after add" packets
  UpdateData update(_mapId);
  update.AddCreateObject(guid, TYPEID_PLAYER, move,
                         BuildPlayerUpdateFields(guid, character));

  MovementInfo itemMove{};
  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32 const itemGuidLow = character.GetVisibleItemGuidLow(slot);
    uint32 const entry = character.GetVisibleItemEntry(slot);
    if (itemGuidLow == 0 || entry == 0)
      continue;
    uint64 const itemOg = MakeItemObjectGuid(itemGuidLow);
    update.AddCreateObject(
        itemOg, TYPEID_ITEM, itemMove,
        BuildItemCreateFields(itemOg, guid, entry,
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
        BuildItemCreateFields(itemOg, guid, entry,
                              character.GetPackItemStackCount(pi)));
  }

  WorldPacket updatePacket(SMSG_UPDATE_OBJECT);
  update.Build(updatePacket);
  SendPacket(updatePacket);

  // Other logged-in players see this client; this client sees them (same map).
  if (auto map = WorldService::Instance().GetMap(_mapId)) {
    map->ForEachPlayer([this, guid, &character, &move](
                           std::shared_ptr<Player> const &other) {
      if (!other || other->GetGuid() == guid)
        return;
      if (auto n = other->GetNotifier())
        SendPlayerCreateToNotifier(n, _mapId, guid, character, move);
      if (auto otherCh = _charService->GetCharacterByGuid(other->GetGuid())) {
        SendPlayerCreateToNotifier(
            std::static_pointer_cast<IMapNotifier>(shared_from_this()), _mapId,
            other->GetGuid(), *otherCh, other->GetPosition());
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

  LOG_INFO("Player {} logged in and spawned at Map {}", guid, _mapId);

  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireEvent("player_login", guid);
  }
}

void WorldSession::HandleLogoutRequest(WorldPacket & /*packet*/) {
  if (_playerGuid == 0) {
    LOG_WARN("CMSG_LOGOUT_REQUEST ignored (not in world)");
    return;
  }

  CancelPeriodicTimeSync();

  uint64 const guid = _playerGuid;
  uint32 const mapId = _mapId;

  // Trinity TCPP order: uint32 reason (0 = OK), uint8 instantLogout.
  // We always allow instant logout (no combat/rest model yet); client returns to
  // character selection after SMSG_LOGOUT_COMPLETE.
  WorldPacket response(SMSG_LOGOUT_RESPONSE, 5);
  response.Append<uint32>(0); // reason
  response.Append<uint8>(1);  // instant logout
  SendPacket(response);

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
  if (!_charService->SaveCharacterOnLogout(_accountId, charGuidLow, mapIdDb,
                                           zoneIdDb, persistPos.x, persistPos.y,
                                           persistPos.z, persistPos.orientation)) {
    LOG_ERROR("SaveCharacterOnLogout failed for guid {}, account {}",
              charGuidLow, _accountId);
  } else {
    LOG_INFO("Saved logout position guid {}: map {} zone {} x={} y={} z={} o={}",
             charGuidLow, mapIdDb, zoneIdDb, persistPos.x, persistPos.y,
             persistPos.z, persistPos.orientation);
  }

  if (auto host = WorldService::Instance().GetScriptHost()) {
    host->FireEvent("player_logout", guid);
  }

  WorldService::Instance().RemovePlayerFromMap(mapId, guid);

  _playerGuid = 0;
  _activeCharacterGuid = 0;
  _playerRace = 0;
  _knownSpells.clear();
  _gcdReady = {};
  _mapId = 0;
  _zoneId = 0;
  _timeSyncNextCounter = 0;
  _position = MovementInfo{};

  WorldPacket complete(SMSG_LOGOUT_COMPLETE, 0);
  SendPacket(complete);
  LOG_INFO("Logout complete for GUID {}, account {} (character select)",
           guid, _accountId);
}

void WorldSession::HandleLogoutCancel(WorldPacket & /*packet*/) {
  // Only meaningful during a timed logout; instant logout never reaches this.
  if (_playerGuid == 0)
    return;

  WorldPacket ack(SMSG_LOGOUT_CANCEL_ACK, 0);
  SendPacket(ack);
}

void WorldSession::HandleQueryNextMailTime(WorldPacket & /*packet*/) {
  // Reference: firelands-cata-ref MailHandler.cpp WorldSession::HandleQueryNextMailTime
  // Sends MSG_QUERY_NEXT_MAIL_TIME back to client.
  WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME, 8);
  data.Append<float>(0.0f);  // next mail time (0 = none)
  data.Append<uint32>(0);    // count
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

  SpellCastWire::ClientCastSpellData c;
  if (!SpellCastWire::TryReadClientCastSpell(packet, c)) {
    LOG_DEBUG(
        "CMSG_CAST_SPELL: unsupported tail or truncated packet (spellId={}, "
        "sendCastFlags=0x{:02X}, readPos={}/{})",
        c.spellId, static_cast<unsigned>(c.sendCastFlags), packet.GetReadPos(),
        packet.Size());
    return;
  }

  uint32 const spellId = static_cast<uint32>(c.spellId);
  auto const known =
      std::find(_knownSpells.begin(), _knownSpells.end(), spellId) !=
      _knownSpells.end();
  if (!known) {
    WorldPacket fail;
    SpellCastWire::BuildSpellFailure(
        fail, _playerGuid, c.castId, c.spellId,
        SpellCastWire::SPELL_FAILED_SPELL_UNAVAILABLE);
    SendPacket(fail);
    return;
  }

  auto const now = std::chrono::steady_clock::now();
  if (now < _gcdReady) {
    WorldPacket fail;
    SpellCastWire::BuildSpellFailure(fail, _playerGuid, c.castId, c.spellId,
                                     SpellCastWire::SPELL_FAILED_NOT_READY);
    SendPacket(fail);
    return;
  }

  uint32 targetFlags = c.targetFlags;
  uint64 targetUnitGuid = _playerGuid;
  if ((c.targetFlags & SpellCastWire::ClientTargetPrimaryGuidMask) == 0) {
    targetFlags = SpellCastWire::TARGET_FLAG_UNIT;
    targetUnitGuid = _playerGuid;
  } else {
    targetUnitGuid =
        c.unitTargetGuid != 0 ? c.unitTargetGuid : _playerGuid;
  }

  uint64 hitGuid = _playerGuid;
  if ((c.targetFlags & SpellCastWire::ClientTargetPrimaryGuidMask) != 0 &&
      c.unitTargetGuid != 0)
    hitGuid = c.unitTargetGuid;

  uint32 const castFlagsStart = SpellCastWire::CAST_FLAG_HAS_TRAJECTORY;
  uint32 const castFlagsGo = SpellCastWire::CAST_FLAG_UNKNOWN_9;
  uint32 const castTimeStart = 0;
  uint32 const castTimeGo = static_cast<uint32>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count());

  WorldPacket start;
  SpellCastWire::BuildSpellStart(start, _playerGuid, c.castId, spellId,
                                 castFlagsStart, 0, castTimeStart, targetFlags,
                                 targetUnitGuid);

  std::vector<uint64> hits = {hitGuid};
  WorldPacket go;
  SpellCastWire::BuildSpellGo(go, _playerGuid, c.castId, spellId, castFlagsGo,
                              0, castTimeGo, hits, targetFlags, targetUnitGuid);

  if (auto map = WorldService::Instance().GetMap(_mapId)) {
    map->BroadcastPacketToNearby(_playerGuid, start, true);
    map->BroadcastPacketToNearby(_playerGuid, go, true);
  } else {
    SendPacket(start);
    SendPacket(go);
  }

  _gcdReady = now + std::chrono::milliseconds(1500);
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
  // 4.3.4 chat UI shows "Name-Realm" when this realm string is non-empty; use
  // empty so only the character name appears (RealmName in yaml is for auth/link).
  AppendPlayerGuidLookupData(response, *chOpt, "");
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
  update.AddValuesUpdate(_playerGuid, BuildPlayerBag0InventoryValues(*refreshed));
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
  update.AddValuesUpdate(_playerGuid, BuildPlayerBag0InventoryValues(*refreshed));
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

} // namespace Firelands
