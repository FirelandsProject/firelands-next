#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/Crypto.h>
#include <shared/Logger.h>
#include <shared/network/BitReader.h>
#include <shared/network/BitWriter.h>

#include <cstring>
#include <vector>

#include <zlib.h>

namespace Firelands {

namespace {

// TrinityCore / TCPP WorldSession.cpp — public key sent when client HasKey is false
// (SMSG_ADDON_INFO secure addon block).
static uint8 const kAddonPublicKey[256] = {
    0xC3, 0x5B, 0x50, 0x84, 0xB9, 0x3E, 0x32, 0x42, 0x8C, 0xD0, 0xC7, 0x48, 0xFA, 0x0E, 0x5D, 0x54,
    0x5A, 0xA3, 0x0E, 0x14, 0xBA, 0x9E, 0x0D, 0xB9, 0x5D, 0x8B, 0xEE, 0xB6, 0x84, 0x93, 0x45, 0x75,
    0xFF, 0x31, 0xFE, 0x2F, 0x64, 0x3F, 0x3D, 0x6D, 0x07, 0xD9, 0x44, 0x9B, 0x40, 0x85, 0x59, 0x34,
    0x4E, 0x10, 0xE1, 0xE7, 0x43, 0x69, 0xEF, 0x7C, 0x16, 0xFC, 0xB4, 0xED, 0x1B, 0x95, 0x28, 0xA8,
    0x23, 0x76, 0x51, 0x31, 0x57, 0x30, 0x2B, 0x79, 0x08, 0x50, 0x10, 0x1C, 0x4A, 0x1A, 0x2C, 0xC8,
    0x8B, 0x8F, 0x05, 0x2D, 0x22, 0x3D, 0xDB, 0x5A, 0x24, 0x7A, 0x0F, 0x13, 0x50, 0x37, 0x8F, 0x5A,
    0xCC, 0x9E, 0x04, 0x44, 0x0E, 0x87, 0x01, 0xD4, 0xA3, 0x15, 0x94, 0x16, 0x34, 0xC6, 0xC2, 0xC3,
    0xFB, 0x49, 0xFE, 0xE1, 0xF9, 0xDA, 0x8C, 0x50, 0x3C, 0xBE, 0x2C, 0xBB, 0x57, 0xED, 0x46, 0xB9,
    0xAD, 0x8B, 0xC6, 0xDF, 0x0E, 0xD6, 0x0F, 0xBE, 0x80, 0xB3, 0x8B, 0x1E, 0x77, 0xCF, 0xAD, 0x22,
    0xCF, 0xB7, 0x4B, 0xCF, 0xFB, 0xF0, 0x6B, 0x11, 0x45, 0x2D, 0x7A, 0x81, 0x18, 0xF2, 0x92, 0x7E,
    0x98, 0x56, 0x5D, 0x5E, 0x69, 0x72, 0x0A, 0x0D, 0x03, 0x0A, 0x85, 0xA2, 0x85, 0x9C, 0xCB, 0xFB,
    0x56, 0x6E, 0x8F, 0x44, 0xBB, 0x8F, 0x02, 0x22, 0x68, 0x63, 0x97, 0xBC, 0x85, 0xBA, 0xA8, 0xF7,
    0xB5, 0x40, 0x68, 0x3C, 0x77, 0x86, 0x6F, 0x4B, 0xD7, 0x88, 0xCA, 0x8A, 0xD7, 0xCE, 0x36, 0xF0,
    0x45, 0x6E, 0xD5, 0x64, 0x79, 0x0F, 0x17, 0xFC, 0x64, 0xDD, 0x10, 0x6F, 0xF3, 0xF5, 0xE0, 0xA6,
    0xC3, 0xFB, 0x1B, 0x8C, 0x29, 0xEF, 0x8E, 0xE5, 0x34, 0xCB, 0xD1, 0x2A, 0xCE, 0x79, 0xC3, 0x9A,
    0x0D, 0x36, 0xEA, 0x01, 0xE0, 0xAA, 0x91, 0x20, 0x54, 0xF0, 0x72, 0xD8, 0x1E, 0xC7, 0x89, 0xD2,
};

static bool ReadAddonCString(std::vector<uint8> const &buf, size_t &pos,
                             std::string &out) {
  out.clear();
  while (pos < buf.size() && buf[pos] != 0)
    out.push_back(static_cast<char>(buf[pos++]));
  if (pos >= buf.size())
    return false;
  ++pos; // NUL
  return true;
}

/// Parses the wire blob from CMSG_AUTH_SESSION (zlib + addon list). See TCPP
/// `WorldSession::ReadAddonsInfo`.
static void TryPopulateAuthAddonsFromWire(std::vector<uint8> const &wire,
                                          std::vector<AuthSecureAddonEntry> &out) {
  out.clear();
  if (wire.size() < 4)
    return;

  uint32 uncompressedSize = 0;
  std::memcpy(&uncompressedSize, wire.data(), 4);
  if (uncompressedSize == 0 || uncompressedSize > 0xFFFFF)
    return;

  std::vector<uint8> dec(uncompressedSize);
  uLongf destLen = uncompressedSize;
  int zrc = ::uncompress(dec.data(), &destLen, wire.data() + 4,
                         static_cast<uLong>(wire.size() - 4));
  if (zrc != Z_OK) {
    LOG_DEBUG("CMSG_AUTH_SESSION addon zlib uncompress failed ({}).", zrc);
    return;
  }
  dec.resize(destLen);

  size_t p = 0;
  if (p + 4 > dec.size())
    return;
  uint32 addonsCount = 0;
  std::memcpy(&addonsCount, dec.data() + p, 4);
  p += 4;

  constexpr uint32 kMaxSecureAddons = 35;
  if (addonsCount > kMaxSecureAddons)
    addonsCount = kMaxSecureAddons;

  for (uint32 i = 0; i < addonsCount; ++i) {
    AuthSecureAddonEntry row;
    if (!ReadAddonCString(dec, p, row.name))
      return;
    if (p >= dec.size())
      return;
    row.hasKey = dec[p++] != 0;
    if (p + 8 > dec.size())
      return;
    p += 8; // publicKeyCrc, urlCrc
    out.push_back(std::move(row));
  }
}

} // namespace

void WorldSession::HandleAuthSession(WorldPacket &packet) {
  uint8 digest[20];
  std::vector<uint8> localChallenge(4);
  uint16 build;
  uint32 realmId;
  int32 loginServerId;

  // 1. Read seeds, digest and build using the Scattered format
  HandleAuthSessionScattered(packet, digest, localChallenge, build, realmId,
                             loginServerId);

  // 2. Addon blob: outer uint32 length, then [uint32 uncompressedLen][zlib...].
  //    Must parse and later echo one SMSG_ADDON_INFO row per secure addon or the
  //    client mis-parses the packet and flags Blizzard_* addons as "Banned".
  _authSecureAddons.clear();
  uint32 addonWireBytes = 0;
  if (packet.GetReadPos() + 4 <= packet.Size()) {
    addonWireBytes = packet.Read<uint32>();
  }
  if (addonWireBytes > 0) {
    if (packet.GetReadPos() + addonWireBytes > packet.Size()) {
      LOG_ERROR("CMSG_AUTH_SESSION: addon blob truncated ({} bytes).",
                addonWireBytes);
      Close();
      return;
    }
    std::vector<uint8> wire(addonWireBytes);
    packet.Read(wire.data(), addonWireBytes);
    TryPopulateAuthAddonsFromWire(wire, _authSecureAddons);
  }

  // 3. Extract Account Name using BitReader (Cataclysm 4.3.4 Build 15595)
  BitReader br(packet);
  br.ReadBit(); // UseIPv6
  uint32 accountNameLength = br.ReadBits(12);
  std::string account = br.ReadString(accountNameLength);

  LOG_DEBUG("CMSG_AUTH_SESSION: Account: '{}', Build: {}, RealmID: {}, "
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
  LOG_DEBUG("[AUTH] WorldCrypt initialized with 40 bytes of K");

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
    LOG_DEBUG("Calculated: {}", Crypto::ToHexString(calculatedDigest));
    LOG_DEBUG("Received:   {}", Crypto::ToHexString(digest, 20));
    Close();
    return;
  }

  _accountId = accountOpt->id;
  LOG_DEBUG("CMSG_AUTH_SESSION: Digest validated successfully for account '{}'.",
            account);

  ReloadGlobalAccountDataFromDb();

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
}

void WorldSession::SendAddonInfo() {
  // TCPP WorldSession::SendAddonsInfo — one block per entry from CMSG_AUTH_SESSION,
  // then uint32 bannedAddonCount (always last in this layout).
  WorldPacket data(SMSG_ADDON_INFO);
  constexpr uint8 kAddonSecureHidden = 2; // Addons::SecureAddonInfo::SECURE_HIDDEN

  for (AuthSecureAddonEntry const &addonInfo : _authSecureAddons) {
    uint8 const status = kAddonSecureHidden;
    uint8 const infoProvided =
        static_cast<uint8>((status != 0u) || addonInfo.hasKey);
    data.Append<uint8>(status);
    data.Append<uint8>(infoProvided);
    if (infoProvided) {
      uint8 const keyProvided = addonInfo.hasKey ? 0 : 1;
      data.Append<uint8>(keyProvided);
      if (!addonInfo.hasKey)
        data.Append(kAddonPublicKey, sizeof(kAddonPublicKey));
      data.Append<uint32>(0); // revision / toc version
    }
    data.Append<uint8>(0); // UrlProvided
  }

  data.Append<uint32>(0); // bannedAddonCount

  SendPacket(data);
}

} // namespace Firelands
