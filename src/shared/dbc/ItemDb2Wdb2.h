#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace Firelands {

/// Client `Item.db2` (WDB2, build > 12880) — same logical row as `ItemEntry` in
/// `firelands-cata-ref/src/server/game/DataStores/DB2Structure.h`:
/// ID, ClassID, SubclassID, SoundOverrideSubclass, Material, DisplayInfoID,
/// InventoryType, SheatheType (all 4-byte fields in the on-disk record).
///
/// Drop `Item.db2` from a 4.3.4.15595 client into `data/dbc/Item.db2` so roster
/// visuals (`SMSG_CHAR_ENUM`) work for any item without a `item_template` row.
class ItemDb2Wdb2 {
public:
  /// @return true if the file was read and at least one row was indexed.
  bool Load(std::string const &path);

  bool IsLoaded() const { return !m_byEntry.empty(); }

  struct DisplayRow {
    uint8_t inventoryType = 0;
    uint32_t displayId = 0;
  };

  std::optional<DisplayRow> Lookup(uint32_t itemEntry) const;

private:
  std::unordered_map<uint32_t, DisplayRow> m_byEntry;
};

} // namespace Firelands
