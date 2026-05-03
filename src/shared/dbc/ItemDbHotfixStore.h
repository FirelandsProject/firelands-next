#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <shared/network/WorldPacket.h>

namespace Firelands {

/// Loads client `Item.db2` / `Item-sparse.db2` from disk and serves `SMSG_DB_REPLY`
/// for `CMSG_DB_QUERY_BULK` when the client asks for rows present in those files.
/// Without this, the server always answers "use embedded DB2" (negative RecordID),
/// which fails for entries not present in the client's embedded DB2 (common for
/// hotfix-only item IDs such as 53008).
class ItemDbHotfixStore {
public:
  /// Tries `Item.db2` plus `Item-sparse.db2` or `ItemSparse.db2` under `dbcDirectory`.
  bool load(std::string const &dbcDirectory);

  bool hasItemTable() const { return !itemById_.empty(); }
  bool hasItemSparseTable() const { return !sparseById_.empty(); }

  /// True if `tableHash` matches the `table_hash` read from our loaded Item / Item-sparse
  /// DB2 files (must match the client's `CMSG_DB_QUERY_BULK` value for that table).
  bool handlesTableHash(uint32_t tableHash) const;

  /// Builds `SMSG_DB_REPLY` with positive RecordID and raw WDB2 row bytes if known.
  /// @return packet to send, or nullopt to fall back to embedded client DB2.
  std::optional<WorldPacket> tryBuildDbReply(uint32_t tableHash,
                                             uint32_t recordId) const;

private:
  std::unordered_map<uint32_t, std::vector<uint8_t>> itemById_;
  std::unordered_map<uint32_t, std::vector<uint8_t>> sparseById_;
  /// From each file's WDB2 header at offset 20 (`wowdev.wiki` db2_header.table_hash).
  uint32_t itemTableHash_ = 0;
  uint32_t sparseTableHash_ = 0;
};

} // namespace Firelands
