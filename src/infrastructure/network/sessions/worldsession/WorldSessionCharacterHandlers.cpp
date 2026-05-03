#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/game/EquipmentCache.h>
#include <shared/Logger.h>
#include <shared/network/BitWriter.h>

namespace Firelands {

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
    // Equipment (VisualItems) — 23 slots for 4.3.4.15595. Per-slot order is
    // invType, displayId, enchantVisual (differs from Trinity 3.3.5 wire order).
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

  if (success) {
    LOG_INFO("Character created: Account={} Name='{}' Race={} Class={}",
             _accountId, name, race, klass);
  } else {
    LOG_WARN("Character creation failed: Account={} Name='{}' Race={} Class={}",
             _accountId, name, race, klass);
  }

  WorldPacket response(SMSG_CHAR_CREATE);
  response.Append<uint8>(success ? 0x2F : 0x30);
  SendPacket(response);
}

void WorldSession::HandleCharDelete(WorldPacket &packet) {
  uint64 guid = packet.Read<uint64>();

  bool success =
      _charService->DeleteCharacter(static_cast<uint32>(guid), _accountId);

  if (success) {
    LOG_INFO("Character deleted: Account={} GUID={}", _accountId, guid);
  } else {
    LOG_WARN("Character deletion failed: Account={} GUID={}", _accountId, guid);
  }

  WorldPacket response(SMSG_CHAR_DELETE);
  response.Append<uint8>(success ? 0x47 : 0x48);
  SendPacket(response);
}

} // namespace Firelands
