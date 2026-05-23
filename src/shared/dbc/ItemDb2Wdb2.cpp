#include <shared/dbc/ItemDb2Wdb2.h>
#include <shared/Logger.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace Firelands {

namespace {

constexpr uint32_t kWdb2Magic =
    (uint32_t('W')) | (uint32_t('D') << 8) | (uint32_t('B') << 16) |
    (uint32_t('2') << 24);

uint32_t ReadU32Le(std::vector<uint8_t> const &data, size_t offset) {
  if (offset + sizeof(uint32_t) > data.size())
    return 0;
  uint32_t v = 0;
  std::memcpy(&v, data.data() + offset, sizeof(v));
  return v;
  }

} // namespace

bool ItemDb2Wdb2::Load(std::string const &path) {
  m_byEntry.clear();

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    LOG_WARN(
        "Item.db2 not found at {} — char roster visuals use `item_template` + "
        "CharStartOutfit.dbc only. Copy client Item.db2 (4.3.4) there for full "
        "coverage.",
        path);
    return false;
  }

  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
  if (raw.size() < 48) {
    LOG_WARN("Item.db2 too small: {}", path);
    return false;
  }

  if (ReadU32Le(raw, 0) != kWdb2Magic) {
    LOG_WARN("Item.db2: not WDB2 (magic mismatch): {}", path);
    return false;
  }

  uint32_t const recordCount = ReadU32Le(raw, 4);
  uint32_t const fieldCount = ReadU32Le(raw, 8);
  uint32_t const recordSize = ReadU32Le(raw, 12);
  uint32_t const stringSize = ReadU32Le(raw, 16);
  uint32_t const build = ReadU32Le(raw, 24);

  size_t pos = 32;
  uint32_t minIndex = 0;
  uint32_t maxIndex = 0;
  if (build > 12880) {
    if (raw.size() < pos + 16) {
      LOG_WARN("Item.db2: truncated extended header: {}", path);
    return false;
  }
    minIndex = ReadU32Le(raw, pos);
    pos += 4;
    maxIndex = ReadU32Le(raw, pos);
    pos += 4;
    pos += 4; // locale
    pos += 4; // unk5
  }

  if (maxIndex != 0) {
    uint64_t const span =
        static_cast<uint64_t>(maxIndex) - static_cast<uint64_t>(minIndex) + 1u;
    uint64_t const skip = span * 4u + span * 2u;
    if (raw.size() < pos + skip) {
      LOG_WARN("Item.db2: truncated index table: {}", path);
    return false;
  }
    pos += static_cast<size_t>(skip);
  }

  if (fieldCount < 7 || recordSize < 28 || recordCount == 0) {
    LOG_WARN(
        "Item.db2: unexpected layout (fields={} recordSize={} count={}): {}",
        fieldCount, recordSize, recordCount, path);
    return false;
  }

  uint64_t const dataBytes =
      static_cast<uint64_t>(recordCount) * static_cast<uint64_t>(recordSize) +
      static_cast<uint64_t>(stringSize);
  if (raw.size() < pos + dataBytes) {
    LOG_WARN("Item.db2: truncated record/string block: {}", path);
    return false;
  }

  // `Itemfmt` "niiiiiii": each column is 4 bytes in the on-disk record
  // for Item.db2 (WDB2); DisplayInfoID = field 5, InventoryType = field 6.
  constexpr size_t kOffId = 0;
  constexpr size_t kOffDisplay = 5u * 4u;
  constexpr size_t kOffInv = 6u * 4u;

  for (uint32_t ri = 0; ri < recordCount; ++ri) {
    size_t const rec = pos + static_cast<size_t>(ri) * static_cast<size_t>(recordSize);
    uint32_t const id = ReadU32Le(raw, rec + kOffId);
    if (id == 0)
      continue;
    uint32_t const displayId = ReadU32Le(raw, rec + kOffDisplay);
    uint32_t const inv = ReadU32Le(raw, rec + kOffInv);
    DisplayRow row{};
    row.displayId = displayId;
    row.inventoryType = static_cast<uint8_t>(std::min<uint32_t>(inv, 255u));
    m_byEntry.insert_or_assign(id, row);
  }

  LOG_DEBUG("Item.db2: indexed {} item entries from {}", m_byEntry.size(), path);
  return !m_byEntry.empty();
  }

std::optional<ItemDb2Wdb2::DisplayRow>
ItemDb2Wdb2::Lookup(uint32_t itemEntry) const {
  auto it = m_byEntry.find(itemEntry);
  if (it == m_byEntry.end())
    return std::nullopt;
  return it->second;
  }

} // namespace Firelands
