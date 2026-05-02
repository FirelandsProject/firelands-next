#ifndef FIRELANDS_SHARED_NETWORK_BIT_READER_H
#define FIRELANDS_SHARED_NETWORK_BIT_READER_H

#include <shared/Common.h>
#include <shared/network/ByteBuffer.h>

#include <cstring>

namespace Firelands {

class BitReader {
public:
  explicit BitReader(ByteBuffer &buffer) : _buffer(buffer), _bitPos(8) {}

  bool ReadBit() {
    if (_bitPos >= 8) {
      _curByte = _buffer.Read<uint8>();
      _bitPos = 0;
    }
    return (_curByte >> (7 - _bitPos++)) & 1;
  }

  uint32 ReadBits(uint8 count) {
    uint32 res = 0;
    for (uint8 i = 0; i < count; ++i) {
      if (ReadBit()) {
        res |= (1 << (count - i - 1));
      }
    }
    return res;
  }

  /// Discard bits until the next byte boundary (WoW bit-packed sections → byte payload).
  void AlignToByteBoundary() {
    while (_bitPos != 8)
      (void)ReadBit();
  }

  /// Little-endian uint32 read from the **current bit stream** (MSB-first within each byte).
  uint32 ReadUInt32LE() {
    uint32 b0 = ReadBits(8);
    uint32 b1 = ReadBits(8);
    uint32 b2 = ReadBits(8);
    uint32 b3 = ReadBits(8);
    return b0 | (b1 << 8u) | (b2 << 16u) | (b3 << 24u);
  }

  int32 ReadInt32LE() {
    uint32 u = ReadUInt32LE();
    int32 i;
    std::memcpy(&i, &u, sizeof(i));
    return i;
  }

  float ReadFloatLE() {
    uint32 u = ReadUInt32LE();
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
  }

  uint8 ReadUInt8Bits() { return static_cast<uint8>(ReadBits(8)); }

  std::string ReadString(uint32 length) {
    std::string res;
    if (length == 0)
      return res;

    // Strings follow bit-length fields on a byte boundary (TCPP `ReadBits` +
    // `ReadString`); align before consuming raw bytes from `ByteBuffer`.
    AlignToByteBoundary();

    std::vector<uint8> bytes(length);
    _buffer.Read(bytes.data(), length);
    res.assign(bytes.begin(), bytes.end());
    return res;
  }

private:
  ByteBuffer &_buffer;
  uint8 _curByte = 0;
  uint8 _bitPos = 8;
};

} // namespace Firelands

#endif
