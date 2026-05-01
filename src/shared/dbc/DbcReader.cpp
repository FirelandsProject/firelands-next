#include <shared/dbc/DbcReader.h>
#include <fstream>
#include <shared/Logger.h>

namespace Firelands {

namespace {
constexpr uint32_t kHeaderSize = 20;
}

std::vector<uint32_t> DbcBuildFieldByteOffsets(std::string_view fmt) {
  const size_t n = fmt.size();
  std::vector<uint32_t> offsets(n);
  if (n == 0)
    return offsets;
  offsets[0] = 0;
  for (size_t i = 1; i < n; ++i) {
    char prev = fmt[i - 1];
    uint32_t inc = ((prev == 'b') || (prev == 'X')) ? 1u : 4u;
    offsets[i] = offsets[i - 1] + inc;
  }
  return offsets;
}

bool DbcReader::Load(std::string const &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    LOG_WARN("DBC not found: {}", path);
    return false;
  }

  m_data.assign(std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());

  if (m_data.size() < kHeaderSize) {
    LOG_WARN("DBC file too small: {}", path);
    return false;
  }

  if (!(m_data[0] == 'W' && m_data[1] == 'D' && m_data[2] == 'B' &&
        m_data[3] == 'C')) {
    LOG_WARN("Invalid DBC header in {}", path);
    return false;
  }

  auto readHeaderU32 = [this](size_t offset) -> uint32_t {
    return static_cast<uint32_t>(m_data[offset]) |
           (static_cast<uint32_t>(m_data[offset + 1]) << 8) |
           (static_cast<uint32_t>(m_data[offset + 2]) << 16) |
           (static_cast<uint32_t>(m_data[offset + 3]) << 24);
  };

  m_recordCount = readHeaderU32(4);
  m_fieldCount = readHeaderU32(8);
  m_recordSize = readHeaderU32(12);
  const uint32_t stringBlockSize = readHeaderU32(16);
  (void)stringBlockSize;

  m_recordsOffset = kHeaderSize;
  const size_t expectedSize =
      m_recordsOffset + static_cast<size_t>(m_recordCount) * m_recordSize;
  if (m_recordCount == 0 || m_fieldCount == 0 || m_recordSize == 0 ||
      m_data.size() < expectedSize) {
    LOG_WARN("Malformed DBC metadata in {}", path);
    return false;
  }

  return true;
}

uint32_t DbcReader::ReadUInt32(uint32_t recordIndex, uint32_t fieldIndex) const {
  if (recordIndex >= m_recordCount)
    return 0;
  const size_t fieldOffset =
      m_recordsOffset + static_cast<size_t>(recordIndex) * m_recordSize +
      static_cast<size_t>(fieldIndex) * sizeof(uint32_t);
  if (fieldOffset + sizeof(uint32_t) > m_data.size())
    return 0;

  return static_cast<uint32_t>(m_data[fieldOffset]) |
         (static_cast<uint32_t>(m_data[fieldOffset + 1]) << 8) |
         (static_cast<uint32_t>(m_data[fieldOffset + 2]) << 16) |
         (static_cast<uint32_t>(m_data[fieldOffset + 3]) << 24);
}

uint8_t DbcReader::ReadUInt8(uint32_t recordIndex, uint32_t fieldIndex,
                             std::vector<uint32_t> const &fieldOffsets) const {
  if (recordIndex >= m_recordCount ||
      fieldIndex >= static_cast<uint32_t>(fieldOffsets.size()))
    return 0;
  const size_t pos =
      m_recordsOffset + static_cast<size_t>(recordIndex) * m_recordSize +
      fieldOffsets[fieldIndex];
  if (pos >= m_data.size())
    return 0;
  return m_data[pos];
}

uint32_t DbcReader::ReadUInt32(uint32_t recordIndex, uint32_t fieldIndex,
                               std::vector<uint32_t> const &fieldOffsets) const {
  if (recordIndex >= m_recordCount ||
      fieldIndex >= static_cast<uint32_t>(fieldOffsets.size()))
    return 0;
  const size_t pos =
      m_recordsOffset + static_cast<size_t>(recordIndex) * m_recordSize +
      fieldOffsets[fieldIndex];
  if (pos + sizeof(uint32_t) > m_data.size())
    return 0;
  return static_cast<uint32_t>(m_data[pos]) |
         (static_cast<uint32_t>(m_data[pos + 1]) << 8) |
         (static_cast<uint32_t>(m_data[pos + 2]) << 16) |
         (static_cast<uint32_t>(m_data[pos + 3]) << 24);
}

int32_t DbcReader::ReadInt32(uint32_t recordIndex, uint32_t fieldIndex,
                             std::vector<uint32_t> const &fieldOffsets) const {
  uint32_t u = ReadUInt32(recordIndex, fieldIndex, fieldOffsets);
  return static_cast<int32_t>(u);
}

} // namespace Firelands
