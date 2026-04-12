#pragma once

#include <shared/network/ByteBuffer.h>
#include <shared/Common.h>

namespace Firelands {

    /**
     * @brief A simple utility to handle bit-packing into a ByteBuffer.
     * Core for Cataclysm 4.3.4 network protocol.
     */
    class BitWriter {
    public:
        explicit BitWriter(ByteBuffer& buffer) : _buffer(buffer), _bitPos(8), _currentByteStep(0) {}

        void WriteBit(bool bit) {
            if (_bitPos == 8) {
                _currentByteStep = _buffer.Size();
                _buffer.Append<uint8>(0);
                _bitPos = 0;
            }
            
            // Note: WoW 4.x BitStream usually packs bits from MSB to LSB within a byte 
            // or follows a specific pattern. For now, we use a standard MSB-first packing.
            if (bit) {
                _buffer[_currentByteStep] |= (1 << (7 - _bitPos));
            }
            
            _bitPos++;
        }

        void WriteBits(uint32 value, uint8 count) {
            for (int i = count - 1; i >= 0; --i) {
                WriteBit((value >> i) & 1);
            }
        }

        void Flush() {
            _bitPos = 8;
        }

    private:
        ByteBuffer& _buffer;
        uint8 _bitPos;
        size_t _currentByteStep;
    };

} // namespace Firelands
