#ifndef FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H
#define FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H

#include <shared/Common.h>
#include <vector>
#include <string>
#include <cstring>

namespace Firelands {

    class ByteBuffer {
    public:
        ByteBuffer() : _readPos(0) {}
        ByteBuffer(size_t res) : _readPos(0) { _storage.reserve(res); }

        void Append(const uint8* src, size_t cnt) {
            _storage.insert(_storage.end(), src, src + cnt);
        }

        template <typename T>
        void Append(T value) {
            Append((uint8*)&value, sizeof(value));
        }

        void WriteString(const std::string& str) {
            Append((uint8*)str.c_str(), str.length());
            Append((uint8)0); // Null terminator
        }

        void WriteString(const char* str) {
            if (str) {
                Append((uint8*)str, std::strlen(str));
            }
            Append((uint8)0); // Null terminator
        }

        /**
         * @brief Writes a string WITHOUT a null terminator.
         * Used in Cataclysm 4.3.4 packets where the string length
         * is already specified in the bitfield header.
         */
        void WriteStringNoNull(const std::string& str) {
            if (!str.empty()) {
                Append((uint8*)str.c_str(), str.length());
            }
        }

        void Append(const std::string& str) {
            WriteString(str);
        }

        void Append(const char* str) {
            WriteString(str);
        }

        template <typename T>
        T Read() {
            if (_readPos + sizeof(T) > _storage.size()) {
                return T(); // Should probably throw or handle error
            }
            T val;
            std::memcpy(&val, &_storage[_readPos], sizeof(T));
            _readPos += sizeof(T);
            return val;
        }

        void Read(uint8* dest, size_t count) {
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
                if (c == 0) break;
                res += c;
            }
            return res;
        }

        uint8* Data() { return _storage.data(); }
        size_t Size() const { return _storage.size(); }
        void Clear() { _storage.clear(); _readPos = 0; }
        
        const uint8* GetBuffer() const { return _storage.data(); }
        
        void Resize(size_t size) { _storage.resize(size); }
        uint8& operator[](size_t pos) { return _storage[pos]; }
        
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

        void WritePackedGuid(uint64 guid) {
            uint8 mask = 0;
            uint8 bytes[8];
            uint8 count = 0;
            
            for (int i = 0; i < 8; ++i) {
                uint8 b = (guid >> (i * 8)) & 0xFF;
                if (b) {
                    mask |= (1 << i);
                    bytes[count++] = b;
                }
            }
            
            Append<uint8>(mask);
            for (int i = 0; i < count; ++i) {
                Append<uint8>(bytes[i]);
            }
        }

    private:
        std::vector<uint8> _storage;
        size_t _readPos;
    };

} // namespace Firelands

#endif // FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H
