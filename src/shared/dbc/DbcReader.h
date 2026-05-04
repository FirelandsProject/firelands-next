#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Firelands {

/// Field start offsets within one record (same rules as FirelandsCore `DBCFileLoader`).
std::vector<uint32_t> DbcBuildFieldByteOffsets(std::string_view fmt);

class DbcReader {
public:
  bool Load(std::string const &path);

  uint32_t GetRecordCount() const { return m_recordCount; }
  uint32_t GetFieldCount() const { return m_fieldCount; }
  uint32_t GetRecordSize() const { return m_recordSize; }

  /// Legacy helper: assumes every column is 4 bytes (wrong for mixed-type DBCs).
  uint32_t ReadUInt32(uint32_t recordIndex, uint32_t fieldIndex) const;

  uint8_t ReadUInt8(uint32_t recordIndex, uint32_t fieldIndex,
                    std::vector<uint32_t> const &fieldOffsets) const;
  uint32_t ReadUInt32(uint32_t recordIndex, uint32_t fieldIndex,
                      std::vector<uint32_t> const &fieldOffsets) const;
  int32_t ReadInt32(uint32_t recordIndex, uint32_t fieldIndex,
                    std::vector<uint32_t> const &fieldOffsets) const;

  float ReadFloat(uint32_t recordIndex, uint32_t fieldIndex,
                  std::vector<uint32_t> const &fieldOffsets) const;

  /// First 4 bytes of the record as IEEE754 LE (used for `gt*` game tables where
  /// `fieldCount`/`recordSize` may not match a single `f` fmt column).
  float ReadFirstFloatInRecord(uint32_t recordIndex) const;

  /// Raw little-endian `uint32` at `byteOffset` from the start of `recordIndex`
  /// (used when a DBC row has mixed `x`/`i` layout but the first columns are stable).
  uint32_t ReadUInt32AtRecordByteOffset(uint32_t recordIndex,
                                        uint32_t byteOffsetFromRecordStart) const;

  bool VerifyFormat(std::string_view fmt) const {
    return static_cast<uint32_t>(fmt.size()) == m_fieldCount;
  }

private:
  std::vector<uint8_t> m_data;
  uint32_t m_recordCount = 0;
  uint32_t m_fieldCount = 0;
  uint32_t m_recordSize = 0;
  size_t m_recordsOffset = 0;
};

} // namespace Firelands
