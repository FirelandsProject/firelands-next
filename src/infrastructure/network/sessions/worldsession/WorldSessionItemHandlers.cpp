#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/Logger.h>
#include <shared/game/InventorySlots.h>
#include <shared/network/BitReader.h>
#include <shared/network/UpdateData.h>
#include <array>
#include <ctime>
#include <vector>

namespace Firelands {

namespace {

bool IsItemDbTableHash(uint32_t tableHash) {
  // `SStrHash("Item")`, `SStrHash("Item-sparse")`, `SStrHash("ItemSparse")` — see wowdev SStrHash / WoWDBDefs manifest.
  return tableHash == 0x50238EC2u || tableHash == 0x6A7C6E76u ||
         tableHash == 0x919BE54Eu;
}

void SendDbReplyUseClientDb2(WorldSession &session, uint32_t tableHash,
                             uint32_t recordId) {
  // Trinity `HandleDBQueryBulk`: negative `RecordID` + empty payload → client uses
  // embedded Item.db2 / Item-sparse.db2 (no server hotfix row).
  WorldPacket reply(SMSG_DB_REPLY);
  int32_t const neg = -static_cast<int32_t>(recordId);
  reply.Append<int32_t>(neg);
  reply.Append<uint32_t>(tableHash);
  reply.Append<uint32_t>(static_cast<uint32_t>(std::time(nullptr)));
  reply.Append<uint32_t>(0);
  session.SendPacket(reply);
}

uint8_t NormalizeBag0ItemSlot(uint8_t bag, uint8_t slot) {
  if (bag != 0 && bag != CLIENT_INVENTORY_SLOT_DEFAULT_BACKPACK)
    return slot;

  // Clients may send backpack slots relative to the 16-slot grid (0..15) for
  // CMSG_AUTO_EQUIP_ITEM; storage uses bag0 absolute inventory slots (23..38).
  if (slot < kPackSlotCount) {
    return static_cast<uint8_t>(INVENTORY_SLOT_ITEM_START + slot);
  }
  return slot;
}

} // namespace

void WorldSession::HandleDbQueryBulk(WorldPacket &packet) {
  if (packet.Size() - packet.GetReadPos() < sizeof(uint32_t))
    return;

  uint32_t const tableHash = packet.Read<uint32_t>();
  BitReader br(packet);
  uint32_t const count = br.ReadBits(23);

  std::vector<std::array<bool, 8>> masks(count);
  for (uint32_t i = 0; i < count; ++i) {
    masks[i][0] = br.ReadBit() != 0;
    masks[i][4] = br.ReadBit() != 0;
    masks[i][7] = br.ReadBit() != 0;
    masks[i][2] = br.ReadBit() != 0;
    masks[i][5] = br.ReadBit() != 0;
    masks[i][3] = br.ReadBit() != 0;
    masks[i][6] = br.ReadBit() != 0;
    masks[i][1] = br.ReadBit() != 0;
  }
  br.AlignToByteBoundary();

  auto skipMaskByte = [&](uint32_t q, int idx) {
    if (masks[q][idx])
      (void)packet.Read<uint8_t>();
  };

  std::vector<uint32_t> recordIds;
  recordIds.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    skipMaskByte(i, 5);
    skipMaskByte(i, 6);
    skipMaskByte(i, 7);
    skipMaskByte(i, 0);
    skipMaskByte(i, 1);
    skipMaskByte(i, 3);
    skipMaskByte(i, 4);
    if (packet.Size() - packet.GetReadPos() < sizeof(uint32_t)) {
      LOG_WARN("HandleDbQueryBulk: truncated at record {}/{}", i, count);
      return;
    }
    recordIds.push_back(packet.Read<uint32_t>());
    skipMaskByte(i, 2);
  }

  if (!IsItemDbTableHash(tableHash)) {
    static uint32_t s_lastUnknownHash = 0;
    if (tableHash != s_lastUnknownHash) {
      s_lastUnknownHash = tableHash;
      LOG_DEBUG("HandleDbQueryBulk: unhandled tableHash=0x{:08X} ({} queries) — no "
                "SMSG_DB_REPLY (non-item DB2)",
                tableHash, count);
    }
    return;
  }

  for (uint32_t entry : recordIds)
    SendDbReplyUseClientDb2(*this, tableHash, entry);
}

void WorldSession::HandleAutoEquipItem(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  if (packet.Size() - packet.GetReadPos() < 2)
    return;

  uint8_t const bag = packet.Read<uint8_t>();
  uint8_t const slot = packet.Read<uint8_t>();
  uint8_t const normalizedSlot = NormalizeBag0ItemSlot(bag, slot);

  LOG_INFO(
      "HandleAutoEquipItem: account={} guid={} bag={} slot={} normalizedSlot={}",
      _accountId, _playerGuid, bag, slot, normalizedSlot);

  if (!_charService->AutoEquipFromBag0(_accountId, static_cast<uint32_t>(_playerGuid),
                                        bag, normalizedSlot)) {
    LOG_INFO("HandleAutoEquipItem: equip rejected (account={} guid={})", _accountId,
             _playerGuid);
    return;
  }

  auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
  if (!refreshed)
    return;

  UpdateData update(_mapId);
  update.AddValuesUpdate(
      _playerGuid,
      WorldSessionObjectUpdate::BuildPlayerBag0InventoryValues(*refreshed));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
}

void WorldSession::HandleUseItem(WorldPacket &packet) {
  if (_playerGuid == 0)
    return;
  // TCPP `WorldPackets::Spells::UseItem::Read()` — first fields used for inventory use.
  if (packet.Size() - packet.GetReadPos() < sizeof(uint8_t) * 3 + sizeof(int32_t))
    return;

  uint8_t const packSlot = packet.Read<uint8_t>();
  uint8_t const slot = packet.Read<uint8_t>();
  uint8_t const normalizedSlot = NormalizeBag0ItemSlot(packSlot, slot);
  (void)packet.Read<uint8_t>(); // Cast.CastID
  (void)packet.Read<int32_t>(); // Cast.SpellID (non-zero for many equippables; TCPP uses spellmgr)
  packet.SetReadPos(packet.Size());

  if (!_charService->AutoEquipFromBag0(_accountId, static_cast<uint32_t>(_playerGuid),
                                        packSlot, normalizedSlot))
    return;

  auto refreshed = _charService->GetCharacterByGuid(_playerGuid);
  if (!refreshed)
    return;

  UpdateData update(_mapId);
  update.AddValuesUpdate(
      _playerGuid,
      WorldSessionObjectUpdate::BuildPlayerBag0InventoryValues(*refreshed));
  WorldPacket pkt(SMSG_UPDATE_OBJECT);
  update.Build(pkt);
  SendPacket(pkt);
}

} // namespace Firelands
