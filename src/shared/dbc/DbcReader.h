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
