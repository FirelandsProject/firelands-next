#pragma once

// Full-file MPQ loader using StormLib.
// Loads the highest-priority version of a file (across patches) into memory
// as a contiguous byte buffer.  Designed for random-access parsing of ADT,
// WDT, WMO, M2, DBC, and similar formats.
//
// Originally ported from reference implementation mpqfile.h/.cpp.

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "StormLib.h"

namespace Firelands::VMap {

class MpqStream {
public:
    // Opens `filename` from the given StormLib archive handle.
    // On failure `IsEof()` returns true; call `ErrorMessage()` for details.
    MpqStream(HANDLE mpq, const char* filename, bool warnIfMissing = true);

    ~MpqStream() = default;

    // Non-copyable.
    MpqStream(MpqStream const&)            = delete;
    MpqStream& operator=(MpqStream const&) = delete;

    // Movable.
    MpqStream(MpqStream&&)            = default;
    MpqStream& operator=(MpqStream&&) = default;

    // Read `bytes` from the current position into `dest`.
    // Returns the number of bytes actually read.
    uint32_t Read(void* dest, uint32_t bytes);

    // Convenience: read a plain-old-data value.
    template<typename T>
    bool Read(T& out) {
        return Read(&out, sizeof(T)) == sizeof(T);
    }

    uint32_t Size()    const { return _size; }
    uint32_t Pos()     const { return _pos;  }
    bool     IsEof()   const { return _eof;  }
    bool     IsValid() const { return !_eof && _buf != nullptr; }

    const uint8_t* Data()    const { return _buf.get(); }
    const uint8_t* Current() const { return _buf.get() + _pos; }

    void Seek(uint32_t offset);
    void SeekRelative(int32_t offset);

    const std::string& ErrorMessage() const { return _error; }

private:
    std::unique_ptr<uint8_t[]> _buf;
    uint32_t                   _size{};
    uint32_t                   _pos{};
    bool                       _eof{true};
    std::string                _error;
};

} // namespace Firelands::VMap
