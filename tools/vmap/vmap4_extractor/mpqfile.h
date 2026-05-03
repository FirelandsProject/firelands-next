#pragma once

// In-memory MPQ file reader (StormLib).
// API-compatible with reference implementation mpqfile.h

#include <cstddef>
#include <cstdint>

#include "StormLib.h"

class MPQFile {
public:
    MPQFile(HANDLE mpq, const char* filename, bool warnNoExist = true);
    ~MPQFile() { close(); }

    MPQFile(MPQFile const&)            = delete;
    MPQFile& operator=(MPQFile const&) = delete;

    size_t read(void* dest, size_t bytes);
    size_t getSize() const { return size_; }
    size_t getPos()  const { return pos_; }
    char*  getBuffer() { return buffer_; }
    char*  getPointer() { return buffer_ + pos_; }
    bool   isEof() const { return eof_; }

    void seek(int offset);
    void seekRelative(int offset);
    void close();

private:
    bool   eof_{true};
    char*  buffer_{nullptr};
    size_t pos_{0};
    size_t size_{0};
};

inline void flipcc(char* fcc) {
    char t = fcc[0];
    fcc[0] = fcc[3];
    fcc[3] = t;
    t = fcc[1];
    fcc[1] = fcc[2];
    fcc[2] = t;
}
