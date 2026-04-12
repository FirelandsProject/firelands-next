#ifndef FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H
#define FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H

#include <shared/Common.h>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

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

        void Append(const std::string& str) {
            Append((uint8*)str.c_str(), str.length());
            Append((uint8)0); // Null terminator
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

    private:
        std::vector<uint8> _storage;
        size_t _readPos;
    };

} // namespace Firelands

#endif // FIRELANDS_SHARED_NETWORK_BYTE_BUFFER_H
