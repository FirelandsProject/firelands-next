#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/Logger.h>
#include <shared/network/AccountDataTypes.h>

#include <ctime>
#include <vector>

#include <zlib.h>

namespace Firelands {

void WorldSession::HandleReadyForAccountDataTimes(WorldPacket & /*packet*/) {
  SendAccountDataTimes(kGlobalAccountDataMask);
}

void WorldSession::ReloadGlobalAccountDataFromDb() {
  _accountData = {};
  _preLoginPerCharAccountDirtyMask = 0;
  if (!_accountDataRepo || _accountId == 0)
    return;
  _accountDataRepo->LoadGlobal(_accountId, _accountData);
}

void WorldSession::ReloadCharacterAccountDataFromDb(uint32 characterGuid) {
  if (!_accountDataRepo)
    return;
  _accountDataRepo->LoadCharacter(characterGuid, _accountData);
}

void WorldSession::HandleUpdateAccountData(WorldPacket &packet) {
  uint32 const type = packet.Read<uint32>();
  uint32 const timestamp = packet.Read<uint32>();
  uint32 const decompressedSize = packet.Read<uint32>();

  auto sendAck = [this, type]() {
    WorldPacket ack(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE);
    ack.Append<uint32>(type);
    ack.Append<uint32>(0);
    SendPacket(ack);
  };

  if (type >= NUM_ACCOUNT_DATA_TYPES) {
    return;
  }

  if (!_accountDataRepo || _accountId == 0) {
    sendAck();
    return;
  }

  if (decompressedSize == 0) {
    _accountData[type].time = 0;
    _accountData[type].data.clear();
    if (IsGlobalAccountDataType(type))
      _accountDataRepo->DeleteGlobal(_accountId, static_cast<uint8_t>(type));
    else if (_activeCharacterGuid != 0)
      _accountDataRepo->DeleteCharacter(_activeCharacterGuid,
                                       static_cast<uint8_t>(type));
    else if (IsPerCharacterAccountDataType(type))
      _preLoginPerCharAccountDirtyMask |= (1u << type);
    sendAck();
    return;
  }

  if (decompressedSize > 0xFFFF) {
    LOG_ERROR("CMSG_UPDATE_ACCOUNT_DATA: decompressedSize {} too large", decompressedSize);
    return;
  }

  size_t const compressedOffset = packet.GetReadPos();
  if (compressedOffset > packet.Size() ||
      (packet.Size() - compressedOffset == 0)) {
    LOG_ERROR("CMSG_UPDATE_ACCOUNT_DATA: missing compressed payload (size={}, "
              "offset={})",
              packet.Size(), compressedOffset);
    return;
  }
  uLongf destLen = decompressedSize;
  std::vector<uint8_t> dest(decompressedSize);
  int const zrc = ::uncompress(
      dest.data(), &destLen, packet.GetBuffer() + compressedOffset,
      static_cast<uLong>(packet.Size() - compressedOffset));
  if (zrc != Z_OK) {
    LOG_ERROR("CMSG_UPDATE_ACCOUNT_DATA: zlib uncompress failed ({})", zrc);
    return;
  }
  dest.resize(static_cast<size_t>(destLen));
  std::string const adata(reinterpret_cast<char const *>(dest.data()),
                          dest.size());

  _accountData[type].time = timestamp;
  _accountData[type].data = adata;
  if (IsGlobalAccountDataType(type))
    _accountDataRepo->UpsertGlobal(_accountId, static_cast<uint8_t>(type),
                                   timestamp, adata);
  else if (_activeCharacterGuid != 0)
    _accountDataRepo->UpsertCharacter(_activeCharacterGuid,
                                     static_cast<uint8_t>(type), timestamp, adata);
  else if (IsPerCharacterAccountDataType(type))
    _preLoginPerCharAccountDirtyMask |= (1u << type);
  sendAck();
}

void WorldSession::HandleRequestAccountData(WorldPacket &packet) {
  uint32 const type = packet.Read<uint32>();
  if (type >= NUM_ACCOUNT_DATA_TYPES || !_accountDataRepo)
    return;

  AccountDataSlot const &slot = _accountData[type];
  uint32 const size = static_cast<uint32>(slot.data.size());
  uLongf destLen = ::compressBound(size);
  std::vector<uint8_t> compressed(static_cast<size_t>(destLen));
  if (size > 0) {
    int const zc = ::compress(
        compressed.data(), &destLen,
        reinterpret_cast<unsigned char const *>(slot.data.data()),
        static_cast<uLong>(size));
    if (zc != Z_OK) {
      LOG_ERROR("CMSG_REQUEST_ACCOUNT_DATA: zlib compress failed ({})", zc);
      return;
    }
  } else {
    destLen = 0;
  }
  compressed.resize(static_cast<size_t>(destLen));

  WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA);
  data.Append<uint64>(_playerGuid);
  data.Append<uint32>(type);
  data.Append<uint32>(slot.time);
  data.Append<uint32>(size);
  if (!compressed.empty())
    data.Append(compressed.data(), compressed.size());
  SendPacket(data);
}

void WorldSession::SendAccountDataTimes(uint32 mask) {
  WorldPacket data(SMSG_ACCOUNT_DATA_TIMES);
  data.Append<uint32>(static_cast<uint32>(std::time(nullptr)));
  data.Append<uint8>(1);
  data.Append<uint32>(mask);
  for (int i = 0; i < NUM_ACCOUNT_DATA_TYPES; ++i) {
    if (mask & (1u << i))
      data.Append<uint32>(_accountData[static_cast<size_t>(i)].time);
  }
  SendPacket(data);
}

} // namespace Firelands
