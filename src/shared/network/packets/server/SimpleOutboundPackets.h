#pragma once

#include <shared/Common.h>
#include <shared/network/BitWriter.h>
#include <shared/network/ServerPacket.h>
#include <shared/network/WorldOpcodes.h>

namespace Firelands::WorldPackets::Misc {

class ClientCacheVersion : public ServerPacket {
public:
  explicit ClientCacheVersion(uint32_t version)
      : ServerPacket(SMSG_CLIENTCACHE_VERSION, 8), _version(version) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint32>(_version);
    return &_worldPacket;
  }

private:
  uint32_t _version;
};

/// `SMSG_LEARNED_DANCE_MOVES` — reference sends uint64(0).
class LearnedDanceMoves : public ServerPacket {
public:
  LearnedDanceMoves() : ServerPacket(SMSG_LEARNED_DANCE_MOVES, 8) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint64>(0);
    return &_worldPacket;
  }
};

class HotfixNotifyBlobEmpty : public ServerPacket {
public:
  HotfixNotifyBlobEmpty() : ServerPacket(SMSG_HOTFIX_NOTIFY_BLOB, 8) {}

  WorldPacket const *Write() override {
    BitWriter bw(_worldPacket);
    bw.WriteBits(0, 22);
    bw.Flush();
    return &_worldPacket;
  }
};

class CalendarSendNumPending : public ServerPacket {
public:
  explicit CalendarSendNumPending(uint32_t count)
      : ServerPacket(SMSG_CALENDAR_SEND_NUM_PENDING, 8), _count(count) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint32>(_count);
    return &_worldPacket;
  }

private:
  uint32_t _count;
};

} // namespace Firelands::WorldPackets::Misc

namespace Firelands::WorldPackets::Social {

/// Empty contact list with `SOCIAL_FLAG_ALL` (reference `PlayerSocial::SendSocialList`).
class ContactListEmpty : public ServerPacket {
public:
  ContactListEmpty()
      : ServerPacket(SMSG_CONTACT_LIST, 16), _flags(0x01u | 0x02u | 0x04u) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint32>(_flags);
    _worldPacket.Append<uint32>(0);
    return &_worldPacket;
  }

private:
  uint32_t _flags;
};

} // namespace Firelands::WorldPackets::Social

namespace Firelands::WorldPackets::Character {

class EquipmentSetListEmpty : public ServerPacket {
public:
  EquipmentSetListEmpty() : ServerPacket(SMSG_EQUIPMENT_SET_LIST, 8) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint32>(0);
    return &_worldPacket;
  }
};

class SetupCurrencyEmpty : public ServerPacket {
public:
  SetupCurrencyEmpty() : ServerPacket(SMSG_SETUP_CURRENCY, 8) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint32>(0);
    return &_worldPacket;
  }
};

class QueryTimeResponse : public ServerPacket {
public:
  QueryTimeResponse(uint32_t serverUnixTime, uint32_t nextDailyReset)
      : ServerPacket(SMSG_QUERY_TIME_RESPONSE, 16), _serverTime(serverUnixTime),
        _nextDailyReset(nextDailyReset) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint32>(_serverTime);
    _worldPacket.Append<uint32>(_nextDailyReset);
    return &_worldPacket;
  }

private:
  uint32_t _serverTime;
  uint32_t _nextDailyReset;
};

class PlayedTime : public ServerPacket {
public:
  PlayedTime(int32_t totalPlayedSec, int32_t levelPlayedSec, uint8_t displayInUi)
      : ServerPacket(SMSG_PLAYED_TIME, 16), _total(totalPlayedSec),
        _level(levelPlayedSec), _display(displayInUi) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<int32>(_total);
    _worldPacket.Append<int32>(_level);
    _worldPacket.Append<uint8>(_display);
    return &_worldPacket;
  }

private:
  int32_t _total;
  int32_t _level;
  uint8_t _display;
};

} // namespace Firelands::WorldPackets::Character

namespace Firelands::WorldPackets::Guild {

class BankMoneyWithdrawn : public ServerPacket {
public:
  explicit BankMoneyWithdrawn(int64 amountCopper)
      : ServerPacket(SMSG_GUILD_BANK_MONEY_WITHDRAWN, 16), _amount(amountCopper) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<int64>(_amount);
    return &_worldPacket;
  }

private:
  int64 _amount;
};

} // namespace Firelands::WorldPackets::Guild

namespace Firelands::WorldPackets::Lfg {

class UpdateStatusNone : public ServerPacket {
public:
  UpdateStatusNone() : ServerPacket(SMSG_LFG_UPDATE_STATUS_NONE, 0) {}

  WorldPacket const *Write() override { return &_worldPacket; }
};

class PlayerInfoEmpty : public ServerPacket {
public:
  PlayerInfoEmpty() : ServerPacket(SMSG_LFG_PLAYER_INFO, 16) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint8>(0);
    _worldPacket.Append<uint32>(0);
    return &_worldPacket;
  }
};

class PartyInfoEmpty : public ServerPacket {
public:
  PartyInfoEmpty() : ServerPacket(SMSG_LFG_PARTY_INFO, 8) {}

  WorldPacket const *Write() override {
    _worldPacket.Append<uint8>(0);
    return &_worldPacket;
  }
};

} // namespace Firelands::WorldPackets::Lfg

namespace Firelands::WorldPackets::WorldState {

class CemeteryListResponseEmpty : public ServerPacket {
public:
  CemeteryListResponseEmpty()
      : ServerPacket(SMSG_REQUEST_CEMETERY_LIST_RESPONSE, 8) {}

  WorldPacket const *Write() override {
    BitWriter bits(_worldPacket);
    bits.WriteBit(false);
    bits.WriteBits(0, 24);
    bits.Flush();
    return &_worldPacket;
  }
};

} // namespace Firelands::WorldPackets::WorldState

namespace Firelands::WorldPackets::Achievement {

class AllDataEmpty : public ServerPacket {
public:
  AllDataEmpty() : ServerPacket(SMSG_ALL_ACHIEVEMENT_DATA, 8) {}

  WorldPacket const *Write() override {
    BitWriter bw(_worldPacket);
    bw.WriteBits(0, 21);
    bw.WriteBits(0, 23);
    bw.Flush();
    return &_worldPacket;
  }
};

} // namespace Firelands::WorldPackets::Achievement
