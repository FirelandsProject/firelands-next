#pragma once

#include <shared/Common.h>
#include <shared/network/ByteBuffer.h>

namespace Firelands {

/**
 * @brief A utility to handle bit-packing into a ByteBuffer.
 * Core for Cataclysm 4.3.4 network protocol.
 *
 * Bits are packed MSB-first (bit 7 down to bit 0) within each byte,
 * matching the reference implementation.
 */
class BitWriter {
public:
  explicit BitWriter(ByteBuffer &buffer)
      : _buffer(buffer), _bitPos(8), _currentByteStep(0) {}

  void WriteBit(bool bit) {
    if (_bitPos == 8) {
      _currentByteStep = _buffer.Size();
      _buffer.Append<uint8>(0);
      _bitPos = 0;
    }

    // Cataclysm 4.3.4: pack bits MSB-first (position 7 down to 0)
    if (bit) {
      _buffer[_currentByteStep] |= (1 << (7 - _bitPos));
    }
    ++_bitPos;
  }

  void WriteBits(uint32 value, uint8 count) {
    // Write most significant bit first
    for (int8 i = static_cast<int8>(count - 1); i >= 0; --i) {
      WriteBit((value >> i) & 1);
    }
  }

  /**
   * @brief Write a GUID mask bit.
   * Writes 1 if the byte at this GUID position is non-zero, 0 otherwise.
   * Used in Cata 4.3.4 scrambled GUID serialization.
   */
  void WriteBitMask(uint8 guidByte) { WriteBit(guidByte != 0); }

  void Flush() { _bitPos = 8; }

private:
  ByteBuffer &_buffer;
  uint8 _bitPos;
  size_t _currentByteStep;
};

} // namespace Firelands
