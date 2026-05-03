#include <shared/dbc/ItemDbHotfixStore.h>
#include <shared/Logger.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
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

/// Same layout walk as `ItemDb2Wdb2::Load`: copy each fixed-size row keyed by first uint32.
bool LoadWdb2Rows(std::string const &path,
                  std::unordered_map<uint32_t, std::vector<uint8_t>> &outById,
                  uint32_t &outTableHash) {
  outTableHash = 0;
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    LOG_WARN("ItemDbHotfixStore: could not open {}", path);
    return false;
  }

  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
  if (raw.size() < 48 || ReadU32Le(raw, 0) != kWdb2Magic) {
    LOG_WARN("ItemDbHotfixStore: invalid or tiny WDB2 file {}", path);
    return false;
  }

  uint32_t const recordCount = ReadU32Le(raw, 4);
  uint32_t const fieldCount = ReadU32Le(raw, 8);
  uint32_t const recordSize = ReadU32Le(raw, 12);
  uint32_t const stringSize = ReadU32Le(raw, 16);
  outTableHash = ReadU32Le(raw, 20);
  uint32_t const build = ReadU32Le(raw, 24);

  size_t pos = 32;
  uint32_t minIndex = 0;
  uint32_t maxIndex = 0;
  if (build > 12880) {
    if (raw.size() < pos + 16) {
      LOG_WARN("ItemDbHotfixStore: truncated extended header in {}", path);
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
      LOG_WARN("ItemDbHotfixStore: truncated index table in {}", path);
      return false;
    }
    pos += static_cast<size_t>(skip);
  }

  if (fieldCount < 1 || recordSize < 4 || recordCount == 0) {
    LOG_WARN("ItemDbHotfixStore: bad header fields in {}", path);
    return false;
  }

  uint64_t const dataBytes =
      static_cast<uint64_t>(recordCount) * static_cast<uint64_t>(recordSize) +
      static_cast<uint64_t>(stringSize);
  if (raw.size() < pos + dataBytes) {
    LOG_WARN("ItemDbHotfixStore: truncated data in {}", path);
    return false;
  }

  size_t const kOffId = 0;
  for (uint32_t ri = 0; ri < recordCount; ++ri) {
    size_t const rec = pos + static_cast<size_t>(ri) * static_cast<size_t>(recordSize);
    uint32_t const id = ReadU32Le(raw, rec + kOffId);
    if (id == 0)
      continue;
    std::vector<uint8_t> row(recordSize);
    std::memcpy(row.data(), raw.data() + rec, recordSize);
    outById.insert_or_assign(id, std::move(row));
  }

  if (outById.empty()) {
    LOG_WARN("ItemDbHotfixStore: no rows indexed from {}", path);
    return false;
  }

  LOG_INFO("ItemDbHotfixStore: loaded {} rows table_hash=0x{:08X} from {}",
           outById.size(), outTableHash, path);
  return true;
}

} // namespace

bool ItemDbHotfixStore::handlesTableHash(uint32_t tableHash) const {
  return (hasItemTable() && tableHash == itemTableHash_) ||
         (hasItemSparseTable() && tableHash == sparseTableHash_);
}

bool ItemDbHotfixStore::load(std::string const &dbcDirectory) {
  itemById_.clear();
  sparseById_.clear();
  itemTableHash_ = 0;
  sparseTableHash_ = 0;

  std::filesystem::path const base(dbcDirectory);

  uint32_t itemTh = 0;
  bool const itemOk = LoadWdb2Rows((base / "Item.db2").string(), itemById_, itemTh);
  if (itemOk)
    itemTableHash_ = itemTh;

  uint32_t sparseTh = 0;
  bool sparseOk = LoadWdb2Rows((base / "Item-sparse.db2").string(), sparseById_, sparseTh);
  if (!sparseOk)
    sparseOk = LoadWdb2Rows((base / "ItemSparse.db2").string(), sparseById_, sparseTh);
  if (sparseOk)
    sparseTableHash_ = sparseTh;

  if (!itemOk)
    LOG_WARN(
        "ItemDbHotfixStore: Item.db2 missing or invalid under {} — item tooltips "
        "for DB-only / hotfix entries will not resolve.",
        dbcDirectory);
  if (!sparseOk)
    LOG_WARN(
        "ItemDbHotfixStore: Item-sparse.db2 not found under {} — names/stats in "
        "tooltips may still fail.",
        dbcDirectory);

  if (itemOk && sparseOk)
    LOG_INFO("ItemDbHotfixStore: Item + Item-sparse ready (hashes 0x{:08X} / 0x{:08X})",
             itemTableHash_, sparseTableHash_);

  return itemOk;
}

std::optional<WorldPacket>
ItemDbHotfixStore::tryBuildDbReply(uint32_t tableHash, uint32_t recordId) const {
  std::vector<uint8_t> const *row = nullptr;
  if (hasItemTable() && tableHash == itemTableHash_) {
    auto it = itemById_.find(recordId);
    if (it != itemById_.end())
      row = &it->second;
  } else if (hasItemSparseTable() && tableHash == sparseTableHash_) {
    auto it = sparseById_.find(recordId);
    if (it != sparseById_.end())
      row = &it->second;
  }

  if (!row || row->empty())
    return std::nullopt;

  WorldPacket reply(SMSG_DB_REPLY, 32 + row->size());
  reply.Append<int32_t>(static_cast<int32_t>(recordId));
  reply.Append<uint32_t>(tableHash);
  reply.Append<uint32_t>(static_cast<uint32_t>(std::time(nullptr)));
  reply.Append<uint32_t>(static_cast<uint32_t>(row->size()));
  reply.Append(row->data(), row->size());
  return reply;
}

} // namespace Firelands
