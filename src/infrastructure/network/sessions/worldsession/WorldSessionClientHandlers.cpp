#include <application/logic/GossipLogic.h>
#include <application/services/WorldService.h>
#include <domain/repositories/IGossipRepository.h>
#include <domain/repositories/INpcTextRepository.h>
#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <shared/game/WowGuid.h>
#include <shared/Logger.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/Permissions.h>
#include <shared/network/BitReader.h>
#include <shared/network/packets/client/PackedPlayerGuidWire.h>
#include <shared/network/packets/client/SessionOpcodesClient.h>
#include <shared/network/packets/server/PongPacket.h>
#include <shared/network/packets/server/SimpleOutboundPackets.h>
#include <shared/network/UpdateData.h>
#include <shared/network/packets/server/NpcTextPackets.h>
#include <shared/network/packets/server/VendorPackets.h>
#include <application/logic/GossipLogic.h>
#include <domain/repositories/IQuestGossipRepository.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <array>
#include <cctype>
#include <ctime>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Firelands {

namespace {

constexpr uint8_t kMailMessageTypeNormal = 0;
constexpr int kMailItemEnchantSlots = 10;
constexpr uint32_t kMailDaySeconds = 86400u;
constexpr uint32_t kMailListMaxShown = 50;

constexpr uint32_t kGmNpcSearchMaxLines = 20;

std::string SanitizeNpcSearchQuery(std::string const &in) {
  std::string out;
  out.reserve(std::min<size_t>(in.size(), 48));
  for (unsigned char uc : in) {
    char const c = static_cast<char>(uc);
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!out.empty() && out.back() != ' ')
        out.push_back(' ');
      continue;
    }
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ||
        c == '\'') {
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (out.size() >= 48)
      break;
  }
  while (!out.empty() && out.back() == ' ')
    out.pop_back();
  return out;
}

void AppendOneMailListEntry(WorldPacket &data, MailInboxRow const &row,
                            std::time_t now) {
  size_t const sizePos = data.Size();
  data.Append<uint16>(0);
  size_t const payloadBegin = data.Size();

  data.Append<uint32>(static_cast<uint32>(row.mailId));
  data.Append<uint8>(kMailMessageTypeNormal);
  // MAIL_NORMAL uses raw 8-byte `ObjectGuid`, not packed.
  data.Append<uint64>(MakePlayerObjectGuid(row.senderGuidLow));
  data.Append<uint64>(0u);       // COD
  data.Append<uint32>(0u);       // package
  data.Append<uint32>(41u);     // stationery (Cataclysm default)
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
    data.Append<uint32>(0);
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

namespace ws_obj = WorldSessionObjectUpdate;

} // namespace

void WorldSession::HandleQueryNextMailTime(WorldPacket & /*packet*/) {
  // Float is negative (e.g. -DAY)
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
  SendPacket(new WorldPackets::Misc::CalendarSendNumPending(0));
}

void WorldSession::HandleZoneUpdate(WorldPacket &packet) {
  WorldPackets::Client::ZoneUpdateRequest z{};
  WorldPackets::Client::ZoneUpdateRequest::Read(packet, z);
  if (z.newZoneId != 0 && z.newZoneId != _zoneId) {
    _zoneId = z.newZoneId;
    if (_playerGuid != 0)
      RefreshPlayerPhaseVisibilityFromAuras();
  }
}

void WorldSession::HandleGuildBankRemainingWithdrawMoneyQuery(WorldPacket & /*packet*/) {
  // Reference: HandleGuildBankMoneyWithdrawn implementation
  // and Guild.cpp Guild::SendMoneyInfo → SMSG_GUILD_BANK_MONEY_WITHDRAWN(int64).
  //
  // We don't implement guilds yet → respond with 0 so the UI doesn't hang.
  SendPacket(new WorldPackets::Guild::BankMoneyWithdrawn(0));
}

void WorldSession::HandleLfgGetStatus(WorldPacket & /*packet*/) {
  // Reference: HandleLfgGetStatus implementation
  // Minimal "not queued / not using LFG" response.
  SendPacket(new WorldPackets::Lfg::UpdateStatusNone());
}

void WorldSession::HandleLfgLockInfoRequest(WorldPacket &packet) {
  // Reference: HandleLfgGetLockInfoOpcode implementation
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
    SendPacket(new WorldPackets::Lfg::PlayerInfoEmpty());
    return;
  }

  // SMSG_LFG_PARTY_INFO:
  // - uint8 playerCount
  // - [blacklist entries...]
  SendPacket(new WorldPackets::Lfg::PartyInfoEmpty());
}

void WorldSession::HandleRequestCemeteryList(WorldPacket & /*packet*/) {
  // Reference: MiscPackets.cpp RequestCemeteryListResponse::Write
  // Layout (bit-packed):
  // - 1 bit  IsGossipTriggered
  // - 24 bits CemeteryID.size()
  // - [uint32 cemeteryId...]
  SendPacket(new WorldPackets::WorldState::CemeteryListResponseEmpty());
}

void WorldSession::HandleNameQuery(WorldPacket &packet) {
  // CMSG_NAME_QUERY: HandleNameQueryOpcode reads ObjectGuid as raw uint64 LE
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

void WorldSession::HandleCreatureQuery(WorldPacket &packet) {
  if (packet.Size() - packet.GetReadPos() < sizeof(uint32))
    return;
  uint32 const entry = packet.Read<uint32>();
  if (packet.GetReadPos() < packet.Size())
    (void)packet.ReadPackedGuid();

  std::optional<std::pair<std::string, std::string>> nameTitle;
  std::array<uint32, 4> creatureModels{};
  if (_npcTemplateSearch) {
    if (auto row =
            _npcTemplateSearch->TryGetByEntry(static_cast<uint32_t>(entry))) {
      nameTitle = std::make_pair(std::move(row->name), std::move(row->subname));
      creatureModels = row->displayIds;
    }
  }

  WorldPacket response;
  ws_obj::BuildCreatureQueryResponse(response, entry, nameTitle, creatureModels);
  SendPacket(response);
}

void WorldSession::SendQueryTimeResponse() {
  SendPacket(new WorldPackets::Character::QueryTimeResponse(
      static_cast<uint32_t>(std::time(nullptr)), 0));
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

  SendPacket(new WorldPackets::Character::PlayedTime(
      0, 0, trigger ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0)));
}

void WorldSession::HandlePing(WorldPacket &packet) {
  WorldPackets::Client::PingRequest ping{};
  WorldPackets::Client::PingRequest::Read(packet, ping);
  SendPacket(new WorldPackets::Misc::Pong(ping.serial));
}

void WorldSession::HandleSetSelection(WorldPacket &packet) {
  // Target guid: Cataclysm normally uses the same bit-packed `ObjectGuid` as
  // `CMSG_PLAYER_LOGIN` (`LoginReadPackedPlayerGuid`). Some clients / docs use a
  // fixed 8-byte little-endian guid body instead; mis-parsing that as bit-packed
  // leaves unread bytes (or yields 0 when the first LE byte is 0x00).
  size_t const start = packet.GetReadPos();
  size_t const rem = packet.Size() - start;
  if (rem < 1) {
    _clientSelectionGuid = 0;
    return;
  }

  uint64 guid = 0;
  WorldPackets::Client::ReadLoginPackedPlayerGuid(packet, guid);

  if (packet.GetReadPos() != packet.Size() && rem == sizeof(uint64)) {
    packet.SetReadPos(start);
    guid = packet.Read<uint64>();
  }

  _clientSelectionGuid = guid;
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
  WorldPackets::Client::TimeSyncResponse ts{};
  WorldPackets::Client::TimeSyncResponse::Read(packet, ts);
  // RESP updates clock skew only; the following
  // SMSG_TIME_SYNC_REQ is sent on a periodic timer — not here.
  LOG_TRACE("CMSG_TIME_SYNC_RESP counter={} clientTime={}", ts.counter,
            ts.clientTime);
}

void WorldSession::HandleMoveTimeSkipped(WorldPacket &packet) {
  uint32 time = packet.Read<uint32>();
  BitReader br(packet);
  for (int i = 0; i < 8; ++i) { if (br.ReadBit()) packet.Read<uint8>(); }
  LOG_DEBUG("CMSG_MOVE_TIME_SKIPPED: Time: {}", time);
}

bool WorldSession::GmNpcSearchPrintResults(std::string const &nameQuery) {
  if (_playerGuid == 0)
    return false;
  if (!_npcTemplateSearch) {
    SendNotification(
        "|cffff5555[NPC search]|r |cffffffffcreature_template|r not available. Run "
        "migrations |cffFFD20023_world_creature_template_search.sql|r / "
        "|cffFFD20024_world_creature_tables.sql|r on |cfffffffffirelands_world|r.");
    return false;
  }

  std::string const q = SanitizeNpcSearchQuery(nameQuery);
  if (q.empty()) {
    SendNotification("|cff664422================================================|r");
    SendNotification("|cffFFD200 NPC template search|r |cffAAAAAA(GM)|r");
    SendNotification("|cff664422------------------------------------------------|r");
    SendNotification("|cffAAAAAAUsage:|r     |cffffffff.npc search <name fragment>|r");
    SendNotification("|cffAAAAAAExample:|r   |cffffffff.npc search kobold|r");
    SendNotification("|cffAAAAAASpawn:|r      |cffffffff.npc add|r |cff00CED1<entry>|r "
                       "|cffAAAAAA<displayId>|r");
    SendNotification("|cff664422================================================|r");
    return true;
  }

  auto rows =
      _npcTemplateSearch->SearchNameSubstring(q, kGmNpcSearchMaxLines + 1u, 0);
  bool truncated = rows.size() > kGmNpcSearchMaxLines;
  if (truncated)
    rows.resize(kGmNpcSearchMaxLines);

  SendNotification("|cff664422================================================|r");
  SendNotification("|cff00FF96NPC search|r  |cff555555>|r  |cffffffff" + q + "|r");
  SendNotification("|cff664422------------------------------------------------|r");

  if (rows.empty()) {
    SendNotification("|cffff9966  No matches.|r Try a shorter or different fragment.");
    SendNotification("|cff664422================================================|r");
    return true;
  }

  uint32_t idx = 0;
  for (NpcTemplateSearchRow const &row : rows) {
    ++idx;
    std::string line = "|cff7EB87C";
    line += std::to_string(idx);
    line += ".|r  ";
    line += "|cff00CED1[";
    line += std::to_string(row.entry);
    line += "]|r  ";
    line += "|cffffffff";
    line += row.name;
    line += "|r";
    if (!row.subname.empty()) {
      line += "  |cff9AA7B8";
      line += row.subname;
      line += "|r";
    }
    line += "   |cffC79C6E.npc add ";
    line += std::to_string(row.entry);
    line += " <displayId>|r";
    if (line.size() > 255)
      line = line.substr(0, 252) + "|r...";
    SendNotification(line);
  }

  SendNotification("|cff664422------------------------------------------------|r");
  SendNotification("|cffAAAAAAShowing:|r |cffffffff" + std::to_string(rows.size()) +
                   "|r rows   "
                   "|cffAAAAAA(|cff00CED1teal brackets|r |cffAAAAAA= template "
                   "entry)|r");
  if (truncated) {
    SendNotification("|cffFF8040  First " + std::to_string(kGmNpcSearchMaxLines) +
                     " rows only — narrow your search to see fewer hits.|r");
  }
  SendNotification("|cff664422================================================|r");
  return true;
}

void WorldSession::HandleGossipHello(WorldPacket &packet) {
  const uint64 npcGuid = ws_obj::ReadClientTargetGuid(packet);
  if (npcGuid == 0 || _playerGuid == 0)
    return;

  _gossipMenuSent = false;

  if (auto host = WorldService::Instance().GetScriptHost())
    host->FireGossipHello(npcGuid);

  if (!_gossipMenuSent && TryOpenQuestGiverDialog(npcGuid))
    return;

  SendGossipComplete();
}

void WorldSession::HandleNpcTextQuery(WorldPacket &packet) {
  if (packet.Size() < sizeof(uint32_t) + sizeof(uint64_t))
    return;

  uint32_t const textId = packet.Read<uint32_t>();
  uint64_t const guid = packet.Read<uint64_t>();

  LOG_DEBUG("CMSG_NPC_TEXT_QUERY textId={} guid={:#x}", textId, guid);

  SendNpcTextForGossipWindow(textId);
}

void WorldSession::HandleListInventory(WorldPacket &packet) {
  uint64_t const vendorGuid = ws_obj::ReadClientTargetGuid(packet);
  if (vendorGuid == 0 || _playerGuid == 0)
    return;

  LOG_DEBUG("CMSG_LIST_INVENTORY vendor={:#x}", vendorGuid);
  SendVendorInventory(vendorGuid);
}

void WorldSession::SendVendorInventory(uint64_t vendorGuid) {
  WorldPacket pkt = vendor_wire::BuildVendorInventory(vendorGuid, {});
  SendPacket(pkt);
}

void WorldSession::HandleGossipSelectOption(WorldPacket &packet) {
  const uint64 npcGuid = ws_obj::ReadClientTargetGuid(packet);
  if (npcGuid == 0 || packet.GetReadPos() + sizeof(uint32) * 2 > packet.Size()) {
    return;
  }
  const uint32 menuId = packet.Read<uint32>();
  const uint32 listId = packet.Read<uint32>();

  std::string code;
  if (packet.GetReadPos() < packet.Size())
    code = packet.ReadString();

  if (TryHandleGmTicketGossipSelect(npcGuid, menuId, listId, code))
    return;

  if (auto host = WorldService::Instance().GetScriptHost())
    host->FireGossipSelect(npcGuid, menuId, listId);

  if (_gossipRepo) {
    auto const options = _gossipRepo->GetMenuOptions(menuId);
    if (auto const *item = FindGossipMenuItem(options, listId)) {
      if (item->actionMenuId != 0) {
        auto const chainedOptions = _gossipRepo->GetMenuOptions(item->actionMenuId);
        auto const textId = _gossipRepo->GetMenuTextId(item->actionMenuId);
        std::vector<GossipQuestItem> quests;
        if (_questGossipRepo) {
          if (auto const entry = TryResolveCreatureTemplateEntry(npcGuid)) {
            auto summaries =
                _questGossipRepo->GetStarterQuestsForCreature(*entry);
            summaries = FilterQuestGossipForPlayer(std::move(summaries),
                                                   _playerClass, _playerRace);
            quests = BuildGossipQuestItems(summaries);
          }
        }
        SendGossipMessage(npcGuid, item->actionMenuId, textId.value_or(0),
                          chainedOptions, quests);
        return;
      }
    }
  }

  SendGossipComplete();
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

} // namespace Firelands
