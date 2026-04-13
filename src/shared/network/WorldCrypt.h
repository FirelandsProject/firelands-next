#pragma once

#include <cstring>
#include <openssl/hmac.h>
#include <shared/Common.h>
#include <vector>

namespace Firelands {

/**
 * @brief Minimal ARC4 (Alleged RC4) stream cipher implementation.
 * Used for encrypting/decrypting WoW packet headers after authentication.
 */
class ARC4 {
public:
  void Init(const uint8 *key, size_t keyLen) {
    for (int i = 0; i < 256; ++i)
      _s[i] = static_cast<uint8>(i);
    uint8 j = 0;
    for (int i = 0; i < 256; ++i) {
      j = j + _s[i] + key[i % keyLen];
      std::swap(_s[i], _s[j]);
    }
    _i = _j = 0;
  }

  void Process(uint8 *data, size_t len) {
    for (size_t k = 0; k < len; ++k) {
      _i = (_i + 1);
      _j = (_j + _s[_i]);
      std::swap(_s[_i], _s[_j]);
      data[k] ^= _s[static_cast<uint8>(_s[_i] + _s[_j])];
    }
  }

private:
  uint8 _s[256]{};
  uint8 _i = 0, _j = 0;
};

/**
 * @brief Handles ARC4 encryption/decryption of WoW packet headers.
 *
 * In Cataclysm 4.3.4, after CMSG_AUTH_SESSION is processed:
 * - SMSG headers (4 bytes: Size[2] + Opcode[2]) are encrypted by the server.
 * - CMSG headers (6 bytes: Size[2] + Opcode[4]) are encrypted by the client.
 * - Packet BODIES are NOT encrypted.
 *
 * Two ARC4 ciphers are used (one per direction), keyed with
 * HMAC-SHA1(SessionKey, Seed). The first 1024 bytes of each ARC4 keystream are
 * dropped.
 */
class WorldCrypt {
public:
  void Init(const std::vector<uint8_t> &sessionKey) {
    // Seeds for Cataclysm 4.x
    static const uint8_t kServerEncryptSeed[] = {
        0x08, 0xF6, 0x61, 0xC1, 0xCA, 0x4C, 0x41, 0xE0,
        0xF2, 0x01, 0x99, 0xFF, 0x02, 0x15, 0x7A, 0x00};
    static const uint8_t kClientDecryptSeed[] = {
        0x40, 0xAD, 0x9C, 0xE3, 0x44, 0x2A, 0x9C, 0x0F,
        0x9F, 0xBE, 0x31, 0xB2, 0xAD, 0x93, 0x9B, 0x61};

    // Derive per-direction keys: HMAC-SHA1(SessionKey, Seed) -> 20-byte key
    uint8_t serverKey[20], clientKey[20];
    unsigned int len = 20;

    HMAC(EVP_sha1(), sessionKey.data(), static_cast<int>(sessionKey.size()),
         kServerEncryptSeed, sizeof(kServerEncryptSeed), serverKey, &len);
    HMAC(EVP_sha1(), sessionKey.data(), static_cast<int>(sessionKey.size()),
         kClientDecryptSeed, sizeof(kClientDecryptSeed), clientKey, &len);

    _encrypt.Init(serverKey, 20);
    _decrypt.Init(clientKey, 20);

    // In Cataclysm 4.3.4 (Build 15595), the 1024-byte keystream drop IS required.
    // WoW uses ARC4-drop1024 to strengthen the cipher.
    uint8 syncBuf[1024];
    std::memset(syncBuf, 0, 1024);
    _encrypt.Process(syncBuf, 1024);
    std::memset(syncBuf, 0, 1024);
    _decrypt.Process(syncBuf, 1024);

    _initialized = true;
  }

  bool IsInitialized() const { return _initialized; }

  /// Encrypt outgoing SMSG header (4 bytes: Size[2 BE] + Opcode[2 LE])
  void EncryptSend(uint8 *header, size_t len) {
    if (!_initialized)
      return;
    _encrypt.Process(header, len);
  }

  /// Decrypt incoming CMSG header (6 bytes: Size[2 BE] + Opcode[4 LE])
  void DecryptRecv(uint8 *header, size_t len) {
    if (!_initialized)
      return;
    _decrypt.Process(header, len);
  }

private:
  ARC4 _encrypt;
  ARC4 _decrypt;
  bool _initialized = false;
};

} // namespace Firelands
