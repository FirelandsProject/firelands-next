#ifndef FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H
#define FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H

#include <cstring>
#include <ctime>
#include <shared/Common.h>
#include <string>
#include <vector>

namespace Firelands {

class ByteBuffer {
public:
  ByteBuffer() : _readPos(0) {}
  ByteBuffer(size_t res) : _readPos(0) { _storage.reserve(res); }

  void Append(const uint8 *src, size_t cnt) {
    if (cnt == 0)
      return;
    _storage.insert(_storage.end(), src, src + cnt);
  }

  template <typename T> void Append(T value) {
    Append((uint8 *)&value, sizeof(value));
  }

  void WriteString(const std::string &str) {
    Append((uint8 *)str.c_str(), str.length());
    Append((uint8)0); // Null terminator
  }

  void WriteString(const char *str) {
    if (str) {
      Append((uint8 *)str, std::strlen(str));
    }
    Append((uint8)0); // Null terminator
  }

  /**
   * @brief Writes a string WITHOUT a null terminator.
   * Used in Cataclysm 4.3.4 packets where the string length
   * is already specified in the bitfield header.
   */
  void WriteStringNoNull(const std::string &str) {
    if (!str.empty()) {
      Append((uint8 *)str.c_str(), str.length());
    }
  }

  /// Length-prefixed UTF-8 with trailing null; length includes the `\0` byte.
  /// Same convention as `AppendSmsgMessageChatPayload` (Cataclysm 4.3.x strings).
  void WriteCataclysmSizedCString(std::string const &str) {
    Append<uint32>(static_cast<uint32>(str.size() + 1));
    WriteString(str);
  }

  void Append(const std::string &str) { WriteString(str); }

  void Append(const char *str) { WriteString(str); }

  template <typename T> T Read() {
    if (_readPos + sizeof(T) > _storage.size()) {
      return T(); // Should probably throw or handle error
    }
    T val;
    std::memcpy(&val, &_storage[_readPos], sizeof(T));
    _readPos += sizeof(T);
    return val;
  }

  void Read(uint8 *dest, size_t count) {
    if (_readPos + count > _storage.size()) {
      std::memset(dest, 0, count);
      return;
    }
    std::memcpy(dest, &_storage[_readPos], count);
    _readPos += count;
  }

  std::string ReadString() {
    std::string res;
    while (_readPos < _storage.size()) {
      char c = Read<char>();
      if (c == 0)
        break;
      res += c;
    }
    return res;
  }

  uint8 *Data() { return _storage.data(); }
  size_t Size() const { return _storage.size(); }
  void Clear() {
    _storage.clear();
    _readPos = 0;
  }

  const uint8 *GetBuffer() const { return _storage.data(); }

  void Resize(size_t size) { _storage.resize(size); }
  uint8 &operator[](size_t pos) { return _storage[pos]; }

  void SetReadPos(size_t pos) { _readPos = pos; }
  size_t GetReadPos() const { return _readPos; }

  /**
   * @brief Writes a single GUID byte in Cataclysm 4.3.4 "ByteSeq" format.
   * Only writes the byte if it is non-zero (the mask bit was set).
   * The byte is XOR'd with 1 per the protocol.
   */
  void WriteByteSeq(uint8 b) {
    if (b != 0) {
      Append<uint8>(b ^ 1);
    }
  }

  /// Cataclysm packed GUID (same as FirelandsCore ByteBuffer::appendPackGUID).
  void AppendPackGUID(uint64 guid) {
    uint8 packGUID[8 + 1] = {};
    size_t size = 1;
    for (uint8 i = 0; guid != 0; ++i) {
      if (guid & 0xFF) {
        packGUID[0] |= static_cast<uint8>(1 << i);
        packGUID[size] = static_cast<uint8>(guid & 0xFF);
        ++size;
      }
      guid >>= 8;
    }
    Append(packGUID, size);
  }

  void WritePackedGuid(uint64 guid) { AppendPackGUID(guid); }

  uint64 ReadPackedGuid() {
    uint8 mask = Read<uint8>();
    uint64 guid = 0;
    for (int i = 0; i < 8; ++i) {
      if (mask & (1 << i)) {
        uint64 b = Read<uint8>();
        guid |= (b << (i * 8));
      }
    }
    return guid;
  }

  /// Overwrite little-endian `uint16` at `byteOffset` (must be within current size).
  void PatchUInt16(size_t byteOffset, uint16_t value) {
    if (byteOffset + sizeof(uint16_t) > _storage.size())
      return;
    std::memcpy(&_storage[byteOffset], &value, sizeof(value));
  }

  void PatchUInt32(size_t byteOffset, uint32_t value) {
    if (byteOffset + sizeof(uint32_t) > _storage.size())
      return;
    std::memcpy(&_storage[byteOffset], &value, sizeof(value));
  }

  void PatchUInt8(size_t byteOffset, uint8_t value) {
    if (byteOffset >= _storage.size())
      return;
    _storage[byteOffset] = value;
  }

  void AppendPackedTime(time_t time) {
    std::tm lt;
#ifdef _WIN32
    localtime_s(&lt, &time);
#else
    localtime_r(&time, &lt);
#endif
    Append<uint32>((lt.tm_year - 100) << 24 | lt.tm_mon << 20 |
                   (lt.tm_mday - 1) << 14 | lt.tm_wday << 11 | lt.tm_hour << 6 |
                   lt.tm_min);
  }

private:
  std::vector<uint8> _storage;
  size_t _readPos;
};

} // namespace Firelands

#endif // FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H
