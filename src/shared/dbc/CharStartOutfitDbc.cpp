#include <shared/dbc/CharStartOutfitDbc.h>
#include <shared/dbc/DbcReader.h>
#include <shared/game/ItemEquipSlots.h>
#include <shared/Logger.h>
#include <string_view>

namespace Firelands {

namespace {
// Must match `firelands-cata-ref/src/server/game/DataStores/DBCfmt.h`
constexpr char kCharStartOutfitFmt[] =
    "dbbbXiiiiiiiiiiiiiiiiiiiiiiiixxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxii";
}

bool CharStartOutfitDbc::Load(std::string const &dbcPath) {
  DbcReader reader;
  if (!reader.Load(dbcPath))
    return false;

  const std::string_view fmt(kCharStartOutfitFmt,
                             sizeof(kCharStartOutfitFmt) - 1);
  if (!reader.VerifyFormat(fmt)) {
    LOG_WARN("CharStartOutfit.dbc field count mismatch (fmt {} vs header {}).",
             fmt.size(), reader.GetFieldCount());
    return false;
  }

  std::vector<uint32_t> const offs = DbcBuildFieldByteOffsets(fmt);
  const uint32_t lastField = static_cast<uint32_t>(fmt.size() - 1);
  const uint32_t expectedRecordSize =
      offs[lastField] +
      (((fmt[lastField] == 'b') || (fmt[lastField] == 'X')) ? 1u : 4u);
  if (expectedRecordSize != reader.GetRecordSize()) {
    LOG_WARN(
        "CharStartOutfit.dbc record size mismatch (computed {} vs header {}).",
        expectedRecordSize, reader.GetRecordSize());
    return false;
  }

  m_visuals.clear();
  m_itemGrants.clear();
  m_itemVisualByEntry.clear();

  for (uint32_t rec = 0; rec < reader.GetRecordCount(); ++rec) {
    uint8_t race = reader.ReadUInt8(rec, 1, offs);
    uint8_t klass = reader.ReadUInt8(rec, 2, offs);
    uint8_t gender = reader.ReadUInt8(rec, 3, offs);

    OutfitKey const key = MakeKey(race, klass, gender);

    std::vector<PlayerCreateVisualItem> visuals;
    std::vector<StarterItemGrant> itemGrants;
    EquipSlotAllocator visualSlotAllocator;

    for (int j = 0; j < 24; ++j) {
      int32_t rawItem = reader.ReadInt32(rec, static_cast<uint32_t>(5 + j), offs);
      int32_t displayRaw =
          reader.ReadInt32(rec, static_cast<uint32_t>(29 + j), offs);
      int32_t invRaw = reader.ReadInt32(rec, static_cast<uint32_t>(53 + j), offs);
      uint8_t invType = invRaw > 0 ? static_cast<uint8_t>(invRaw) : 0;
      if (rawItem > 0) {
        StarterItemGrant grant;
        grant.itemId = static_cast<uint32_t>(rawItem);
        grant.count = 0; // resolved later from DB proto when available
        grant.invType = invType;
        itemGrants.push_back(grant);
        if (displayRaw > 0 && invRaw > 0) {
          m_itemVisualByEntry.try_emplace(
              grant.itemId,
              ItemVisualInfo{invType, static_cast<uint32_t>(displayRaw)});
        }
      }

      // DBC uses -1 for "unused". Treat <= 0 as empty.
      if (displayRaw <= 0)
        continue;
      if (invRaw <= 0)
        continue;

      auto slot = visualSlotAllocator.TryEquipSlot(invType);
      if (!slot)
        continue;

      PlayerCreateVisualItem row;
      row.slot = *slot;
      row.displayId = static_cast<uint32_t>(displayRaw);
      row.invType = invType;
      row.displayEnchantId = 0;
      visuals.push_back(row);
    }

    m_visuals[key] = std::move(visuals);
    m_itemGrants[key] = std::move(itemGrants);
  }

  LOG_INFO("Loaded CharStartOutfit.dbc: {} outfit keys.", m_visuals.size());
  return true;
}

std::vector<PlayerCreateVisualItem>
CharStartOutfitDbc::GetVisualItems(uint8 race, uint8 klass,
                                   uint8 gender) const {
  auto it = m_visuals.find(MakeKey(race, klass, gender));
  if (it != m_visuals.end())
    return it->second;
  return {};
}

std::vector<StarterItemGrant>
CharStartOutfitDbc::GetStarterItemGrants(uint8 race, uint8 klass,
                                         uint8 gender) const {
  auto it = m_itemGrants.find(MakeKey(race, klass, gender));
  if (it != m_itemGrants.end())
    return it->second;
  return {};
}

std::optional<CharStartOutfitDbc::ItemVisualInfo>
CharStartOutfitDbc::GetItemVisualByEntry(uint32 itemEntry) const {
  auto const it = m_itemVisualByEntry.find(itemEntry);
  if (it == m_itemVisualByEntry.end())
    return std::nullopt;
  return it->second;
}

} // namespace Firelands
