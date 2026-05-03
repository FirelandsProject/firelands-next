#include "MySqlCharacterRepository.h"
#include <domain/models/Character.h>
#include <domain/models/PlayerCreateInfo.h>
#include <shared/game/Bag0InventoryData.h>
#include <shared/game/EquipmentCache.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/ItemEquipSlots.h>
#include <shared/Logger.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Firelands {

namespace {

struct ItemProtoRow {
  uint8 inventoryType = 0;
  uint32 buyCount = 1;
  uint32 displayId = 0;
};

float FiniteOrZero(float v) { return std::isfinite(v) ? v : 0.f; }

/// `ResultSet::getFloat` can throw on some REAL values MariaDB returns in scientific
/// notation (e.g. subnormals). `getDouble` accepts those wire forms.
float ResultSetWorldFloat(sql::ResultSet &rs, char const *column) {
  try {
    double const d = rs.getDouble(column);
    float const f = static_cast<float>(d);
    return std::isfinite(f) ? f : 0.f;
  } catch (sql::SQLException &) {
    return 0.f;
  }
}

bool IsMissingTableError(sql::SQLException &e) {
  return e.getErrorCode() == 1146 || e.getSQLState() == "42S02";
}

bool EnsureCharactersOrientationColumn(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "ALTER TABLE `firelands_characters`.`characters` "
        "ADD COLUMN `orientation` float NOT NULL DEFAULT 0 AFTER `z`");
    LOG_DEBUG(
        "Added missing column `firelands_characters.characters.orientation`.");
    return true;
  } catch (sql::SQLException &e) {
    if (e.getErrorCode() == 1060)
      return true; // ER_DUP_FIELDNAME — already present
    std::string const msg{e.what()};
    if (msg.find("Duplicate column") != std::string::npos)
      return true;
    LOG_WARN("EnsureCharactersOrientationColumn failed: {}", e.what());
    return false;
  }
}

bool EnsureCharactersMoneyColumn(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "ALTER TABLE `firelands_characters`.`characters` "
        "ADD COLUMN `money` int unsigned NOT NULL DEFAULT 0 AFTER `firstLogin`");
    LOG_DEBUG("Added missing column `firelands_characters.characters.money`.");
    return true;
  } catch (sql::SQLException &e) {
    if (e.getErrorCode() == 1060)
      return true;
    std::string const msg{e.what()};
    if (msg.find("Duplicate column") != std::string::npos)
      return true;
    LOG_WARN("EnsureCharactersMoneyColumn failed: {}", e.what());
    return false;
  }
}

bool EnsureCharacterSpellTable(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "CREATE TABLE IF NOT EXISTS firelands_characters.character_spell ("
        "guid INT UNSIGNED NOT NULL,"
        "spell INT UNSIGNED NOT NULL,"
        "PRIMARY KEY (guid, spell),"
        "KEY idx_guid (guid),"
        "CONSTRAINT fk_character_spell_guid FOREIGN KEY (guid) REFERENCES "
        "characters(guid) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    return true;
  } catch (sql::SQLException &e) {
    LOG_ERROR("EnsureCharacterSpellTable failed: {}", e.what());
    return false;
  }
}

bool EnsureStarterInventoryTables(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "CREATE TABLE IF NOT EXISTS firelands_characters.item_instance ("
        "guid INT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "itemEntry INT UNSIGNED NOT NULL DEFAULT 0,"
        "owner_guid INT UNSIGNED NOT NULL DEFAULT 0,"
        "creatorGuid INT UNSIGNED NOT NULL DEFAULT 0,"
        "giftCreatorGuid INT UNSIGNED NOT NULL DEFAULT 0,"
        "count INT UNSIGNED NOT NULL DEFAULT 1,"
        "duration INT NOT NULL DEFAULT 0,"
        "charges TINYTEXT,"
        "flags INT UNSIGNED NOT NULL DEFAULT 0,"
        // VARCHAR allows DEFAULT on MariaDB; TEXT/BLOB cannot.
        "enchantments VARCHAR(4096) NOT NULL DEFAULT '',"
        "randomPropertyType TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        "randomPropertyId INT UNSIGNED NOT NULL DEFAULT 0,"
        "durability SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        "creationTime INT UNSIGNED NOT NULL DEFAULT 0,"
        "`text` TEXT NULL,"
        "PRIMARY KEY (guid),"
        "KEY idx_owner_guid (owner_guid)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    st->execute(
        "CREATE TABLE IF NOT EXISTS firelands_characters.character_inventory ("
        "guid INT UNSIGNED NOT NULL DEFAULT 0,"
        "bag INT UNSIGNED NOT NULL DEFAULT 0,"
        "slot TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        "item INT UNSIGNED NOT NULL DEFAULT 0,"
        "PRIMARY KEY (item),"
        "UNIQUE KEY guid (guid, bag, slot),"
        "KEY idx_guid (guid)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    return true;
  } catch (sql::SQLException &e) {
    LOG_ERROR("EnsureStarterInventoryTables failed: {}", e.what());
    return false;
  }
}

bool EnsureMailTables(std::shared_ptr<sql::Connection> conn) {
  try {
    std::unique_ptr<sql::Statement> st(conn->createStatement());
    st->execute(
        "CREATE TABLE IF NOT EXISTS firelands_characters.mail ("
        "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "receiver_guid INT UNSIGNED NOT NULL,"
        "sender_guid INT UNSIGNED NOT NULL DEFAULT 0,"
        "subject VARCHAR(200) NOT NULL DEFAULT 'Item delivery',"
        "body TEXT NULL,"
        "deliver_time INT UNSIGNED NOT NULL DEFAULT 0,"
        "expire_time INT UNSIGNED NOT NULL DEFAULT 0,"
        "checked TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        "PRIMARY KEY (id),"
        "KEY idx_receiver (receiver_guid),"
        "CONSTRAINT fk_mail_receiver FOREIGN KEY (receiver_guid) REFERENCES "
        "characters(guid) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    st->execute(
        "CREATE TABLE IF NOT EXISTS firelands_characters.mail_items ("
        "mail_id BIGINT UNSIGNED NOT NULL,"
        "item_guid INT UNSIGNED NOT NULL,"
        "receiver_guid INT UNSIGNED NOT NULL,"
        "PRIMARY KEY (item_guid),"
        "KEY idx_mail (mail_id),"
        "CONSTRAINT fk_mail_items_mail FOREIGN KEY (mail_id) REFERENCES mail(id) "
        "ON DELETE CASCADE,"
        "CONSTRAINT fk_mail_items_receiver FOREIGN KEY (receiver_guid) REFERENCES "
        "characters(guid) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    return true;
  } catch (sql::SQLException &e) {
    LOG_ERROR("EnsureMailTables failed: {}", e.what());
    return false;
  }
}

bool SaveInventoryToDb(std::shared_ptr<sql::Connection> conn, uint32_t charGuid,
                  Bag0InventoryData const &invData) {
  if (!EnsureStarterInventoryTables(conn))
    return false;

  try {
    conn->setAutoCommit(false);

    {
      std::shared_ptr<sql::PreparedStatement> delInv(conn->prepareStatement(
          "DELETE FROM character_inventory WHERE guid = ? AND bag = 0"));
      delInv->setUInt(1, charGuid);
      delInv->executeUpdate();
    }
    {
      std::shared_ptr<sql::PreparedStatement> delItem(conn->prepareStatement(
          "DELETE FROM item_instance WHERE owner_guid = ? AND guid NOT IN "
          "(SELECT item FROM character_inventory WHERE guid = ?)"));
      delItem->setUInt(1, charGuid);
      delItem->setUInt(2, charGuid);
      delItem->executeUpdate();
    }

    for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
      uint32_t entry = invData.equipEntries[slot];
      uint32_t guid = invData.equipGuids[slot];
      uint32_t count = invData.equipStacks[slot];
      if (entry == 0 || guid == 0)
        continue;

      std::shared_ptr<sql::PreparedStatement> insItem(conn->prepareStatement(
          "INSERT INTO item_instance (guid, itemEntry, owner_guid, count, "
          "enchantments) VALUES (?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE "
          "itemEntry = VALUES(itemEntry), count = VALUES(count)"));
      insItem->setUInt(1, guid);
      insItem->setUInt(2, entry);
      insItem->setUInt(3, charGuid);
      insItem->setUInt(4, count > 0 ? count : 1);
      insItem->setString(5, "");
      insItem->executeUpdate();

      std::shared_ptr<sql::PreparedStatement> insInv(conn->prepareStatement(
          "INSERT INTO character_inventory (guid, bag, slot, item) "
          "VALUES (?, 0, ?, ?) ON DUPLICATE KEY UPDATE slot = VALUES(slot)"));
      insInv->setUInt(1, charGuid);
      insInv->setUInt(2, static_cast<unsigned>(slot));
      insInv->setUInt(3, guid);
      insInv->executeUpdate();
    }

    for (size_t pi = 0; pi < kPackSlotCount; ++pi) {
      uint32_t entry = invData.packEntries[pi];
      uint32_t guid = invData.packGuids[pi];
      uint32_t count = invData.packStacks[pi];
      if (entry == 0 || guid == 0)
        continue;

      uint8_t slot = static_cast<uint8_t>(INVENTORY_SLOT_ITEM_START + pi);

      std::shared_ptr<sql::PreparedStatement> insItem(conn->prepareStatement(
          "INSERT INTO item_instance (guid, itemEntry, owner_guid, count, "
          "enchantments) VALUES (?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE "
          "itemEntry = VALUES(itemEntry), count = VALUES(count)"));
      insItem->setUInt(1, guid);
      insItem->setUInt(2, entry);
      insItem->setUInt(3, charGuid);
      insItem->setUInt(4, count > 0 ? count : 1);
      insItem->setString(5, "");
      insItem->executeUpdate();

      std::shared_ptr<sql::PreparedStatement> insInv(conn->prepareStatement(
          "INSERT INTO character_inventory (guid, bag, slot, item) "
          "VALUES (?, 0, ?, ?) ON DUPLICATE KEY UPDATE slot = VALUES(slot)"));
      insInv->setUInt(1, charGuid);
      insInv->setUInt(2, static_cast<unsigned>(slot));
      insInv->setUInt(3, guid);
      insInv->executeUpdate();
    }

    conn->commit();
    conn->setAutoCommit(true);
    return true;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("SaveInventory failed for guid {}: {}", charGuid, e.what());
    try {
      conn->rollback();
      conn->setAutoCommit(true);
    } catch (...) {
    }
    return false;
  }
}

std::optional<ItemProtoRow> FetchItemProto(
    std::shared_ptr<sql::Connection> conn, uint32_t itemEntry,
    CharStartOutfitDbc const *charStartOutfitDbc,
    ItemDb2Wdb2 const *itemDb2) {
  try {
    std::shared_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "SELECT InventoryType AS ity, BuyCount AS bct, displayid AS did "
        "FROM firelands_world.item_template WHERE entry = ? LIMIT 1"));
    ps->setUInt(1, itemEntry);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    if (rs->next()) {
      ItemProtoRow row;
      row.inventoryType = static_cast<uint8>(rs->getUInt("ity"));
      row.buyCount =
          std::max(1u, static_cast<uint32_t>(rs->getInt("bct")));
      row.displayId = rs->getUInt("did");
      return row;
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("FetchItemProto failed for entry {}: {}", itemEntry, e.what());
  }
  if (itemDb2 && itemDb2->IsLoaded()) {
    if (auto client = itemDb2->Lookup(itemEntry)) {
      ItemProtoRow row;
      row.inventoryType = client->inventoryType;
      row.buyCount = 1u;
      row.displayId = client->displayId;
      return row;
    }
  }
  if (charStartOutfitDbc) {
    if (auto visual = charStartOutfitDbc->GetItemVisualByEntry(itemEntry)) {
      ItemProtoRow row;
      row.inventoryType = visual->invType;
      row.buyCount = 1;
      row.displayId = visual->displayId;
      return row;
    }
  }
  return std::nullopt;
}

std::optional<uint8_t> PrimaryEquipSlotForInventoryType(uint8_t inventoryType) {
  switch (static_cast<ItemInventoryType>(inventoryType)) {
  case INVTYPE_HEAD:
    return 0;
  case INVTYPE_NECK:
    return 1;
  case INVTYPE_SHOULDERS:
    return 2;
  case INVTYPE_BODY:
    return 3;
  case INVTYPE_CHEST:
  case INVTYPE_ROBE:
    return 4;
  case INVTYPE_WAIST:
    return 5;
  case INVTYPE_LEGS:
    return 6;
  case INVTYPE_FEET:
    return 7;
  case INVTYPE_WRISTS:
    return 8;
  case INVTYPE_HANDS:
    return 9;
  case INVTYPE_FINGER:
    return 10;
  case INVTYPE_TRINKET:
    return 12;
  case INVTYPE_CLOAK:
    return 14;
  case INVTYPE_WEAPON:
  case INVTYPE_2HWEAPON:
  case INVTYPE_WEAPONMAINHAND:
    return 15;
  case INVTYPE_SHIELD:
  case INVTYPE_WEAPONOFFHAND:
  case INVTYPE_HOLDABLE:
    return 16;
  case INVTYPE_RANGED:
  case INVTYPE_THROWN:
  case INVTYPE_RANGEDRIGHT:
    return 17;
  case INVTYPE_TABARD:
    return 18;
  default:
    return std::nullopt;
  }
}

/// Prefer a free finger/trinket slot; otherwise swap onto the primary slot for that pair.
std::optional<uint8_t> PickAutoEquipDestination(Bag0InventoryData const &bag0,
                                                 uint8_t inventoryType) {
  std::optional<uint8_t> const primary =
      PrimaryEquipSlotForInventoryType(inventoryType);
  if (!primary)
    return std::nullopt;

  auto slotEmpty = [&](unsigned s) -> bool {
    return s < kEquipmentSlotCount && bag0.equipEntries[s] == 0;
  };

  if (inventoryType == INVTYPE_FINGER) {
    if (slotEmpty(10))
      return 10;
    if (slotEmpty(11))
      return 11;
    return 10;
  }
  if (inventoryType == INVTYPE_TRINKET) {
    if (slotEmpty(12))
      return 12;
    if (slotEmpty(13))
      return 13;
    return 12;
  }

  return primary;
}

Bag0InventoryData LoadBag0Inventory(std::shared_ptr<sql::Connection> conn,
                                     uint32_t charGuid) {
  Bag0InventoryData out;
  if (!EnsureStarterInventoryTables(conn))
    return out;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "SELECT ci.slot, ci.item, ii.itemEntry, ii.count FROM character_inventory ci "
        "INNER JOIN item_instance ii ON ii.guid = ci.item "
        "WHERE ci.guid = ? AND ci.bag = 0 AND (ci.slot < ? OR (ci.slot >= ? AND ci.slot < ?))"));
    ps->setUInt(1, charGuid);
    ps->setUInt(2, static_cast<unsigned>(EQUIPMENT_SLOT_END));
    ps->setUInt(3, static_cast<unsigned>(INVENTORY_SLOT_ITEM_START));
    ps->setUInt(4, static_cast<unsigned>(INVENTORY_SLOT_ITEM_END));
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    while (rs->next()) {
      unsigned slot = rs->getUInt("slot");
      uint32_t entry = rs->getUInt("itemEntry");
      uint32_t ig = rs->getUInt("item");
      uint32_t count =
          std::max(1u, static_cast<uint32_t>(rs->getUInt("count")));
      if (slot < kEquipmentSlotCount) {
        out.equipEntries[slot] = entry;
        out.equipGuids[slot] = ig;
        out.equipStacks[slot] = count;
      } else if (slot >= INVENTORY_SLOT_ITEM_START &&
                 slot < INVENTORY_SLOT_ITEM_END) {
        size_t const pi =
            static_cast<size_t>(slot - INVENTORY_SLOT_ITEM_START);
        out.packEntries[pi] = entry;
        out.packGuids[pi] = ig;
        out.packStacks[pi] = count;
      }
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("LoadBag0Inventory failed for guid {}: {}", charGuid, e.what());
  }
  return out;
}

/// Roster (`SMSG_CHAR_ENUM`) visuals: `character_inventory` + `item_template`, then
/// optional client `Item.db2`, then CharStartOutfit.dbc.
std::string BuildCharEnumEquipmentFromInventory(
    Bag0InventoryData const &bag0,
    std::shared_ptr<sql::Connection> itemProtoConn,
    std::unordered_map<uint32_t, ItemProtoRow> &protoByEntry,
    CharStartOutfitDbc const *charStartOutfitDbc,
    ItemDb2Wdb2 const *itemDb2) {
  EquipmentCache::VisualArray merged{};
  for (size_t slot = 0; slot < kEquipmentSlotCount; ++slot) {
    uint32_t const entry = bag0.equipEntries[slot];
    if (entry == 0)
      continue;

    ItemProtoRow proto{};
    auto cached = protoByEntry.find(entry);
    if (cached != protoByEntry.end()) {
      proto = cached->second;
    } else if (auto fetched = FetchItemProto(itemProtoConn, entry,
                                             charStartOutfitDbc, itemDb2)) {
      proto = *fetched;
      protoByEntry.emplace(entry, proto);
    } else {
      continue;
    }

    merged[slot].invType = proto.inventoryType;
    merged[slot].displayId = proto.displayId;
    merged[slot].displayEnchantId = 0;
  }

  return EquipmentCache::SerializeVisualArray(merged);
}

/// Snapshot of `characters` row — must be read fully before any other query on the same
/// connection; nested statements while a `ResultSet` is open leave behavior undefined.
struct AccountCharacterRow {
  uint32_t guid = 0;
  uint32_t account = 0;
  std::string name;
  uint8_t race = 0;
  uint8_t klass = 0;
  uint8_t gender = 0;
  uint8_t skin = 0;
  uint8_t face = 0;
  uint8_t hairStyle = 0;
  uint8_t hairColor = 0;
  uint8_t facialHair = 0;
  uint8_t level = 0;
  uint16_t zoneId = 0;
  uint16_t mapId = 0;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float orientation = 0.f;
  uint32_t guildId = 0;
  uint32_t characterFlags = 0;
  uint32_t customizationFlags = 0;
  bool firstLogin = false;
  uint8_t outfitId = 0;
  std::string dbEquipmentCache;
  uint32_t money = 0;
};

} // namespace

MySqlCharacterRepository::MySqlCharacterRepository(
    std::shared_ptr<sql::Connection> characterConnection,
    std::shared_ptr<sql::Connection> worldConnection)
    : _connection(std::move(characterConnection)),
      _worldConnection(std::move(worldConnection)) {
  EnsureCharactersOrientationColumn(_connection);
  EnsureCharactersMoneyColumn(_connection);
  EnsureCharacterSpellTable(_connection);
  _charStartOutfitLoaded =
      _charStartOutfitDbc.Load("data/dbc/CharStartOutfit.dbc");
  if (!_charStartOutfitLoaded) {
    LOG_WARN("MySqlCharacterRepository: could not load data/dbc/CharStartOutfit.dbc; "
             "item visual fallback disabled.");
  }
  _itemDb2.Load("data/dbc/Item.db2");
}

std::vector<std::shared_ptr<Character>>
MySqlCharacterRepository::GetCharactersByAccount(uint32_t accountId) {
  std::vector<std::shared_ptr<Character>> characters;
  try {
    std::unordered_map<uint32_t, ItemProtoRow> itemProtoByEntry;
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "SELECT guid, account, name, race, class, gender, skin, face, "
        "hairStyle, hairColor, facialHair, outfitId, equipmentCache, "
        "level, zoneId, mapId, x, y, z, orientation, guildId, characterFlags, "
        "customizationFlags, firstLogin, money "
        "FROM characters WHERE account = ?"));
    stmnt->setUInt(1, accountId);

    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

    std::vector<AccountCharacterRow> rows;
    while (res->next()) {
      AccountCharacterRow row;
      row.guid = res->getUInt("guid");
      row.account = res->getUInt("account");
      row.name = std::string(res->getString("name"));
      row.race = static_cast<uint8>(res->getUInt("race"));
      row.klass = static_cast<uint8>(res->getUInt("class"));
      row.gender = static_cast<uint8>(res->getUInt("gender"));
      row.skin = static_cast<uint8>(res->getUInt("skin"));
      row.face = static_cast<uint8>(res->getUInt("face"));
      row.hairStyle = static_cast<uint8>(res->getUInt("hairStyle"));
      row.hairColor = static_cast<uint8>(res->getUInt("hairColor"));
      row.facialHair = static_cast<uint8>(res->getUInt("facialHair"));
      row.level = static_cast<uint8>(res->getUInt("level"));
      row.zoneId = static_cast<uint16>(res->getUInt("zoneId"));
      row.mapId = static_cast<uint16>(res->getUInt("mapId"));
      row.x = ResultSetWorldFloat(*res, "x");
      row.y = ResultSetWorldFloat(*res, "y");
      row.z = ResultSetWorldFloat(*res, "z");
      row.orientation = ResultSetWorldFloat(*res, "orientation");
      row.guildId = res->getUInt("guildId");
      row.characterFlags = res->getUInt("characterFlags");
      row.customizationFlags = res->getUInt("customizationFlags");
      row.firstLogin = res->getBoolean("firstLogin");
      row.outfitId = static_cast<uint8>(res->getUInt("outfitId"));
      row.dbEquipmentCache =
          res->isNull("equipmentCache") ? ""
                                        : std::string(res->getString("equipmentCache"));
      row.money = res->getUInt("money");
      rows.push_back(std::move(row));
    }
    res.reset();

    for (AccountCharacterRow const &r : rows) {
      Bag0InventoryData const bag0 = LoadBag0Inventory(_connection, r.guid);
      std::string const equipmentCacheForRoster =
          BuildCharEnumEquipmentFromInventory(
              bag0, itemTemplateConnection(), itemProtoByEntry,
              _charStartOutfitLoaded ? &_charStartOutfitDbc : nullptr,
              &_itemDb2);

      characters.push_back(std::make_shared<Character>(
          r.guid, r.account, r.name, r.race, r.klass, r.gender, r.skin, r.face,
          r.hairStyle, r.hairColor, r.facialHair, r.level, r.zoneId, r.mapId,
          r.x, r.y, r.z, r.orientation, r.guildId, r.characterFlags,
          r.customizationFlags, r.firstLogin, r.outfitId, equipmentCacheForRoster,
          std::array<uint32_t, kEquipmentSlotCount>{},
          std::array<uint32_t, kEquipmentSlotCount>{},
          std::array<uint32_t, kEquipmentSlotCount>{},
          std::array<uint32_t, kPackSlotCount>{},
          std::array<uint32_t, kPackSlotCount>{},
          std::array<uint32_t, kPackSlotCount>{},
          r.money));
    }
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in GetCharactersByAccount: {}", e.what());
  }
  return characters;
}

std::optional<uint32_t>
MySqlCharacterRepository::CreateCharacter(const Character &character) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "INSERT INTO characters (account, name, race, class, gender, skin, "
        "face, hairStyle, hairColor, facialHair, outfitId, equipmentCache, "
        "level, zoneId, mapId, x, y, z, orientation, guildId, characterFlags, "
        "customizationFlags, firstLogin, money) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    stmnt->setUInt(1, character.GetAccount());
    stmnt->setString(2, character.GetName());
    stmnt->setUInt(3, character.GetRace());
    stmnt->setUInt(4, character.GetClass());
    stmnt->setUInt(5, character.GetGender());
    stmnt->setUInt(6, character.GetSkin());
    stmnt->setUInt(7, character.GetFace());
    stmnt->setUInt(8, character.GetHairStyle());
    stmnt->setUInt(9, character.GetHairColor());
    stmnt->setUInt(10, character.GetFacialHair());
    stmnt->setUInt(11, character.GetOutfitId());
    stmnt->setString(12, character.GetEquipmentCache());
    stmnt->setUInt(13, character.GetLevel());
    stmnt->setUInt(14, character.GetZoneId());
    stmnt->setUInt(15, character.GetMapId());
    stmnt->setDouble(16, static_cast<double>(character.GetX()));
    stmnt->setDouble(17, static_cast<double>(character.GetY()));
    stmnt->setDouble(18, static_cast<double>(character.GetZ()));
    stmnt->setDouble(19, static_cast<double>(character.GetOrientation()));
    stmnt->setUInt(20, character.GetGuildId());
    stmnt->setUInt(21, character.GetCharacterFlags());
    stmnt->setUInt(22, character.GetCustomizationFlags());
    stmnt->setBoolean(23, character.IsFirstLogin());
    stmnt->setUInt(24, character.GetMoney());

    stmnt->executeUpdate();

    std::unique_ptr<sql::Statement> idStmt(_connection->createStatement());
    std::unique_ptr<sql::ResultSet> idRs(
        idStmt->executeQuery("SELECT LAST_INSERT_ID()"));
    if (!idRs->next())
      return std::nullopt;
    return static_cast<uint32_t>(idRs->getUInt(1));
  } catch (sql::SQLException &e) {
    LOG_ERROR(
        "Database error in CreateCharacter: {} (SQLState: {}, ErrorCode: {})",
        e.what(), e.getSQLState().c_str(), e.getErrorCode());
    return std::nullopt;
  }
}

bool MySqlCharacterRepository::GrantStarterItems(
    uint32_t characterGuid, std::vector<StarterItemGrant> const &items) {
  if (items.empty())
    return true;
  if (!EnsureStarterInventoryTables(_connection))
    return false;

  EquipSlotAllocator equipAllocator;
  std::unordered_set<uint8_t> usedPackSlots;

  try {
    _connection->setAutoCommit(false);

    for (StarterItemGrant grant : items) {
      if (grant.itemId == 0)
        continue;

      auto proto = FetchItemProto(itemTemplateConnection(), grant.itemId,
                                  _charStartOutfitLoaded ? &_charStartOutfitDbc
                                                         : nullptr,
                                  &_itemDb2);
      uint8_t inventoryType = 0;
      uint32_t count = grant.count;
      if (proto) {
        inventoryType = proto->inventoryType;
        if (count == 0)
          count = proto->buyCount;
      } else {
        inventoryType = grant.invType;
        if (count == 0)
          count = 1;
        LOG_WARN("GrantStarterItems: item {} missing template/proto, using "
                 "DBC fallback invType={}.",
                 grant.itemId, static_cast<unsigned>(inventoryType));
      }
      count = std::max(1u, count);

      uint8_t bag = 0;
      uint8_t slot = 0;
      bool placed = false;

      if (!grant.bagOnly) {
        if (auto equipSlot = equipAllocator.TryEquipSlot(inventoryType)) {
          slot = *equipSlot;
          placed = true;
        }
      }
      if (!placed) {
        for (unsigned s = INVENTORY_SLOT_ITEM_START;
             s < INVENTORY_SLOT_ITEM_END; ++s) {
          uint8_t bs = static_cast<uint8_t>(s);
          if (!usedPackSlots.count(bs)) {
            slot = bs;
            usedPackSlots.insert(bs);
            placed = true;
            break;
          }
        }
      }

      if (!placed) {
        LOG_WARN("GrantStarterItems: no space for item {}", grant.itemId);
        continue;
      }

      {
        std::shared_ptr<sql::PreparedStatement> insItem(
            _connection->prepareStatement(
                "INSERT INTO item_instance (itemEntry, owner_guid, creatorGuid, "
                "giftCreatorGuid, count, duration, charges, flags, enchantments, "
                "randomPropertyType, randomPropertyId, durability, creationTime, "
                "text) VALUES (?, ?, 0, 0, ?, 0, '', 0, '', 0, 0, 0, "
                "UNIX_TIMESTAMP(), NULL)"));
        insItem->setUInt(1, grant.itemId);
        insItem->setUInt(2, characterGuid);
        insItem->setUInt(3, count);
        insItem->executeUpdate();
      }

      uint64_t itemGuid = 0;
      {
        std::unique_ptr<sql::Statement> st(_connection->createStatement());
        std::unique_ptr<sql::ResultSet> rs(
            st->executeQuery("SELECT LAST_INSERT_ID()"));
        if (!rs->next()) {
          LOG_ERROR("GrantStarterItems: LAST_INSERT_ID failed.");
          _connection->rollback();
          _connection->setAutoCommit(true);
          return false;
        }
        itemGuid = rs->getUInt64(1);
      }

      std::shared_ptr<sql::PreparedStatement> insInv(
          _connection->prepareStatement(
              "INSERT INTO character_inventory (guid, bag, slot, item) VALUES "
              "(?, ?, ?, ?)"));
      insInv->setUInt(1, characterGuid);
      insInv->setUInt(2, bag);
      insInv->setUInt(3, slot);
      insInv->setUInt64(4, itemGuid);
      insInv->executeUpdate();
    }

    _connection->commit();
    _connection->setAutoCommit(true);
    return true;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("GrantStarterItems failed: {}", e.what());
    try {
      _connection->rollback();
      _connection->setAutoCommit(true);
    } catch (...) {
    }
    return false;
  }
}

bool MySqlCharacterRepository::DeleteCharacter(uint32_t guid,
                                               uint32_t accountId) {
  try {
    try {
      std::shared_ptr<sql::PreparedStatement> delInv(
          _connection->prepareStatement(
              "DELETE FROM character_inventory WHERE guid = ?"));
      delInv->setUInt(1, guid);
      delInv->executeUpdate();

      std::shared_ptr<sql::PreparedStatement> delItems(
          _connection->prepareStatement(
              "DELETE FROM item_instance WHERE owner_guid = ?"));
      delItems->setUInt(1, guid);
      delItems->executeUpdate();
    } catch (sql::SQLException &e) {
      if (!IsMissingTableError(e))
        throw;
      LOG_WARN("DeleteCharacter: starter inventory tables missing ({}). "
               "Proceeding with character row deletion only.",
               e.what());
    }

    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "DELETE FROM characters WHERE guid = ? AND account = ?"));
    stmnt->setUInt(1, guid);
    stmnt->setUInt(2, accountId);
    stmnt->executeUpdate();
    return true;
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in DeleteCharacter: {}", e.what());
    return false;
  }
}

bool MySqlCharacterRepository::IsNameAvailable(const std::string &name) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "SELECT 1 FROM characters WHERE name = ?"));
    stmnt->setString(1, name);
    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());
    return !res->next();
  } catch (sql::SQLException &e) {
    LOG_ERROR("Database error in IsNameAvailable: {}", e.what());
    return false;
  }
}

std::optional<Character>
MySqlCharacterRepository::GetCharacterByGuid(uint64_t guid) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "SELECT guid, account, name, race, class, gender, skin, face, "
        "hairStyle, hairColor, facialHair, outfitId, equipmentCache, "
        "level, zoneId, mapId, x, y, z, orientation, guildId, characterFlags, "
        "customizationFlags, firstLogin, money "
        "FROM characters WHERE guid = ?"));
    stmnt->setUInt64(1, guid);

    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

    if (res->next()) {
      uint32_t const lowGuid = res->getUInt("guid");
      uint32_t const account = res->getUInt("account");
      std::string const name = std::string(res->getString("name"));
      uint8_t const race = static_cast<uint8>(res->getUInt("race"));
      uint8_t const klass = static_cast<uint8>(res->getUInt("class"));
      uint8_t const gender = static_cast<uint8>(res->getUInt("gender"));
      uint8_t const skin = static_cast<uint8>(res->getUInt("skin"));
      uint8_t const face = static_cast<uint8>(res->getUInt("face"));
      uint8_t const hairStyle = static_cast<uint8>(res->getUInt("hairStyle"));
      uint8_t const hairColor = static_cast<uint8>(res->getUInt("hairColor"));
      uint8_t const facialHair = static_cast<uint8>(res->getUInt("facialHair"));
      uint8_t const level = static_cast<uint8>(res->getUInt("level"));
      uint16_t const zoneId = static_cast<uint16>(res->getUInt("zoneId"));
      uint16_t const mapId = static_cast<uint16>(res->getUInt("mapId"));
      float const px = ResultSetWorldFloat(*res, "x");
      float const py = ResultSetWorldFloat(*res, "y");
      float const pz = ResultSetWorldFloat(*res, "z");
      float const po = ResultSetWorldFloat(*res, "orientation");
      uint32_t const guildId = res->getUInt("guildId");
      uint32_t const characterFlags = res->getUInt("characterFlags");
      uint32_t const customizationFlags = res->getUInt("customizationFlags");
      bool const firstLogin = res->getBoolean("firstLogin");
      uint8_t const outfitId = static_cast<uint8>(res->getUInt("outfitId"));
      std::string const equipmentCache =
          res->isNull("equipmentCache") ? ""
                                        : std::string(res->getString("equipmentCache"));
      uint32_t const money = res->getUInt("money");
      // Read the full row before nested queries; do not `res.reset()` here — closing
      // the result set early has been observed to upset the same connection/session
      // for the inventory query on some MariaDB connector builds.

      Bag0InventoryData const bag0 = LoadBag0Inventory(_connection, lowGuid);
      return Character(lowGuid, account, name, race, klass, gender, skin, face,
                       hairStyle, hairColor, facialHair, level, zoneId, mapId, px,
                       py, pz, po, guildId, characterFlags, customizationFlags,
                       firstLogin, outfitId, equipmentCache, bag0.equipEntries,
                       bag0.equipGuids, bag0.equipStacks, bag0.packEntries,
                       bag0.packGuids, bag0.packStacks, money);
    }
    return std::nullopt;
  } catch (sql::SQLException &e) {
    LOG_ERROR("SQLException in GetCharacterByGuid: {}", e.what());
    return std::nullopt;
  }
}

bool MySqlCharacterRepository::SaveCharacterOnLogout(
    uint32_t accountId, uint32_t characterGuid, uint16_t mapId, uint16_t zoneId,
    float x, float y, float z, float orientation, uint32_t moneyCopper) {
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "UPDATE characters SET mapId = ?, zoneId = ?, x = ?, y = ?, z = ?, "
        "orientation = ?, firstLogin = 0, money = ? WHERE guid = ? AND account = ?"));
    stmnt->setUInt(1, mapId);
    stmnt->setUInt(2, zoneId);
    stmnt->setDouble(3, static_cast<double>(FiniteOrZero(x)));
    stmnt->setDouble(4, static_cast<double>(FiniteOrZero(y)));
    stmnt->setDouble(5, static_cast<double>(FiniteOrZero(z)));
    stmnt->setDouble(6, static_cast<double>(FiniteOrZero(orientation)));
    stmnt->setUInt(7, moneyCopper);
    stmnt->setUInt(8, characterGuid);
    stmnt->setUInt(9, accountId);
    return stmnt->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("SaveCharacterOnLogout failed: {}", e.what());
    return false;
  }
}

bool MySqlCharacterRepository::UpdateCharacterMoney(uint32_t accountId,
                                                    uint32_t characterGuid,
                                                    uint32_t moneyCopper) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE characters SET money = ? WHERE guid = ? AND account = ?"));
    st->setUInt(1, moneyCopper);
    st->setUInt(2, characterGuid);
    st->setUInt(3, accountId);
    return st->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("UpdateCharacterMoney failed: {}", e.what());
    return false;
  }
}

bool MySqlCharacterRepository::UpdateCharacterLevel(uint32_t accountId,
                                                    uint32_t characterGuid,
                                                    uint8_t level) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(_connection->prepareStatement(
        "UPDATE characters SET level = ? WHERE guid = ? AND account = ?"));
    st->setUInt(1, level);
    st->setUInt(2, characterGuid);
    st->setUInt(3, accountId);
    return st->executeUpdate() > 0;
  } catch (sql::SQLException &e) {
    LOG_ERROR("UpdateCharacterLevel failed: {}", e.what());
    return false;
  }
}

std::vector<uint32_t>
MySqlCharacterRepository::GetCharacterSpellIds(uint32_t characterGuid) {
  std::vector<uint32_t> out;
  if (!EnsureCharacterSpellTable(_connection))
    return out;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(
        _connection->prepareStatement("SELECT spell FROM character_spell WHERE "
                                      "guid = ?"));
    ps->setUInt(1, characterGuid);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    while (rs->next())
      out.push_back(rs->getUInt("spell"));
  } catch (sql::SQLException const &e) {
    LOG_WARN("GetCharacterSpellIds failed: {}", e.what());
  }
  return out;
}

bool MySqlCharacterRepository::AddCharacterSpell(uint32_t characterGuid,
                                                uint32_t spellId) {
  if (spellId == 0)
    return false;
  if (!EnsureCharacterSpellTable(_connection))
    return false;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "INSERT IGNORE INTO character_spell (guid, spell) VALUES (?, ?)"));
    ps->setUInt(1, characterGuid);
    ps->setUInt(2, spellId);
    ps->executeUpdate();
    return true;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("AddCharacterSpell failed: {}", e.what());
    return false;
  }
}

bool MySqlCharacterRepository::HasItemTemplate(uint32_t itemEntry) const {
  if (itemEntry == 0)
    return false;
  return FetchItemProto(itemTemplateConnection(), itemEntry,
                        _charStartOutfitLoaded ? &_charStartOutfitDbc : nullptr,
                        &_itemDb2)
      .has_value();
}

bool MySqlCharacterRepository::GrantItemToBag0(uint32_t characterGuid,
                                               uint32_t itemEntry, uint32_t count,
                                               uint32_t *outItemGuidLow,
                                               uint8_t *outBag0Slot) {
  if (itemEntry == 0 || count == 0)
    return false;
  if (!EnsureStarterInventoryTables(_connection))
    return false;

  auto proto = FetchItemProto(itemTemplateConnection(), itemEntry,
                              _charStartOutfitLoaded ? &_charStartOutfitDbc : nullptr,
                              &_itemDb2);
  if (!proto) {
    LOG_WARN("GrantItemToBag0: item {} has no template (item_template / DB2 / outfit).",
             itemEntry);
    return false;
  }

  std::unordered_set<uint8_t> usedPackSlots;
  try {
    std::shared_ptr<sql::PreparedStatement> qslots(
        _connection->prepareStatement(
            "SELECT slot FROM character_inventory WHERE guid = ? AND bag = 0"));
    qslots->setUInt(1, characterGuid);
    std::unique_ptr<sql::ResultSet> rs(qslots->executeQuery());
    while (rs->next()) {
      unsigned const s = rs->getUInt("slot");
      if (s >= INVENTORY_SLOT_ITEM_START && s < INVENTORY_SLOT_ITEM_END)
        usedPackSlots.insert(static_cast<uint8_t>(s));
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("GrantItemToBag0 slot scan failed: {}", e.what());
  }

  uint32_t grantCount = count;
  if (grantCount == 0)
    grantCount = proto->buyCount;
  grantCount = std::max(1u, grantCount);

  uint8_t bag = 0;
  uint8_t slot = 0;
  bool placed = false;
  for (unsigned s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s) {
    uint8_t bs = static_cast<uint8_t>(s);
    if (!usedPackSlots.count(bs)) {
      slot = bs;
      placed = true;
      break;
    }
  }
  if (!placed) {
    LOG_WARN("GrantItemToBag0: no free backpack slot for item {}", itemEntry);
    return false;
  }

  try {
    std::shared_ptr<sql::PreparedStatement> insItem(_connection->prepareStatement(
        "INSERT INTO item_instance (itemEntry, owner_guid, creatorGuid, "
        "giftCreatorGuid, count, duration, charges, flags, enchantments, "
        "randomPropertyType, randomPropertyId, durability, creationTime, "
        "text) VALUES (?, ?, 0, 0, ?, 0, '', 0, '', 0, 0, 0, UNIX_TIMESTAMP(), NULL)"));
    insItem->setUInt(1, itemEntry);
    insItem->setUInt(2, characterGuid);
    insItem->setUInt(3, grantCount);
    insItem->executeUpdate();

    uint64_t itemGuid = 0;
    {
      std::unique_ptr<sql::Statement> st(_connection->createStatement());
      std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT LAST_INSERT_ID()"));
      if (!rs->next())
        return false;
      itemGuid = rs->getUInt64(1);
    }

    std::shared_ptr<sql::PreparedStatement> insInv(
        _connection->prepareStatement(
            "INSERT INTO character_inventory (guid, bag, slot, item) VALUES "
            "(?, ?, ?, ?)"));
    insInv->setUInt(1, characterGuid);
    insInv->setUInt(2, bag);
    insInv->setUInt(3, slot);
    insInv->setUInt64(4, itemGuid);
    insInv->executeUpdate();
    if (outItemGuidLow)
      *outItemGuidLow = static_cast<uint32_t>(itemGuid);
    if (outBag0Slot)
      *outBag0Slot = slot;
    return true;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("GrantItemToBag0 failed: {}", e.what());
    return false;
  }
}

bool MySqlCharacterRepository::SendGmMailWithItem(uint32_t receiverCharacterGuid,
                                                 uint32_t itemEntry,
                                                 uint32_t count) {
  if (receiverCharacterGuid == 0 || itemEntry == 0 || count == 0)
    return false;
  if (!EnsureStarterInventoryTables(_connection) || !EnsureMailTables(_connection))
    return false;

  auto proto = FetchItemProto(itemTemplateConnection(), itemEntry,
                              _charStartOutfitLoaded ? &_charStartOutfitDbc : nullptr,
                              &_itemDb2);
  if (!proto) {
    LOG_WARN("SendGmMailWithItem: item {} has no template (item_template / DB2 / outfit).",
             itemEntry);
    return false;
  }
  uint32_t grantCount = count;
  if (grantCount == 0)
    grantCount = proto->buyCount;
  grantCount = std::max(1u, grantCount);

  try {
    std::shared_ptr<sql::PreparedStatement> insItem(_connection->prepareStatement(
        "INSERT INTO item_instance (itemEntry, owner_guid, creatorGuid, "
        "giftCreatorGuid, count, duration, charges, flags, enchantments, "
        "randomPropertyType, randomPropertyId, durability, creationTime, "
        "text) VALUES (?, ?, 0, 0, ?, 0, '', 0, '', 0, 0, 0, UNIX_TIMESTAMP(), NULL)"));
    insItem->setUInt(1, itemEntry);
    insItem->setUInt(2, receiverCharacterGuid);
    insItem->setUInt(3, grantCount);
    insItem->executeUpdate();

    uint64_t itemGuid = 0;
    {
      std::unique_ptr<sql::Statement> st(_connection->createStatement());
      std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT LAST_INSERT_ID()"));
      if (!rs->next())
        return false;
      itemGuid = rs->getUInt64(1);
    }

    std::string const body =
        "Your backpack was full; this item was attached to this mail message.";
    std::shared_ptr<sql::PreparedStatement> insMail(_connection->prepareStatement(
        "INSERT INTO mail (receiver_guid, sender_guid, subject, body, deliver_time, "
        "expire_time, checked) VALUES (?, 0, 'Item delivery', ?, UNIX_TIMESTAMP(), "
        "0, 0)"));
    insMail->setUInt(1, receiverCharacterGuid);
    insMail->setString(2, body);
    insMail->executeUpdate();

    uint64_t mailId = 0;
    {
      std::unique_ptr<sql::Statement> st(_connection->createStatement());
      std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT LAST_INSERT_ID()"));
      if (!rs->next())
        return false;
      mailId = rs->getUInt64(1);
    }

    std::shared_ptr<sql::PreparedStatement> insLink(_connection->prepareStatement(
        "INSERT INTO mail_items (mail_id, item_guid, receiver_guid) VALUES (?, ?, ?)"));
    insLink->setUInt64(1, mailId);
    insLink->setUInt64(2, itemGuid);
    insLink->setUInt(3, receiverCharacterGuid);
    insLink->executeUpdate();
    return true;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("SendGmMailWithItem failed: {}", e.what());
    return false;
  }
}

bool MySqlCharacterRepository::AutoEquipFromBag0Slot(
    uint32_t characterGuid, uint8_t srcSlot,
    std::optional<uint8_t> fallbackInventoryType) {
  if (srcSlot < INVENTORY_SLOT_ITEM_START || srcSlot >= INVENTORY_SLOT_ITEM_END) {
    LOG_DEBUG("AutoEquipFromBag0Slot: guid={} invalid srcSlot={}", characterGuid,
              srcSlot);
    return false;
  }
  auto bag0 = LoadBag0Inventory(_connection, characterGuid);
  size_t const pi = static_cast<size_t>(srcSlot - INVENTORY_SLOT_ITEM_START);
  uint32_t const entry = bag0.packEntries[pi];
  if (entry == 0) {
    LOG_DEBUG("AutoEquipFromBag0Slot: guid={} empty bag slot={}", characterGuid,
              srcSlot);
    return false;
  }
  auto proto = FetchItemProto(itemTemplateConnection(), entry,
                              _charStartOutfitLoaded ? &_charStartOutfitDbc : nullptr,
                              &_itemDb2);
  uint8_t inventoryType = 0;
  if (proto) {
    inventoryType = proto->inventoryType;
  } else if (fallbackInventoryType && *fallbackInventoryType != 0) {
    inventoryType = *fallbackInventoryType;
    LOG_DEBUG(
        "AutoEquipFromBag0Slot: guid={} entry={} using fallback inventoryType={}",
        characterGuid, entry, static_cast<uint32_t>(inventoryType));
  } else {
    LOG_DEBUG("AutoEquipFromBag0Slot: guid={} entry={} missing item proto",
              characterGuid, entry);
    return false;
  }

  auto dstOpt = PickAutoEquipDestination(bag0, inventoryType);
  if (!dstOpt) {
    LOG_DEBUG(
        "AutoEquipFromBag0Slot: guid={} entry={} inventoryType={} has no equip slot",
        characterGuid, entry, static_cast<uint32_t>(inventoryType));
    return false;
  }
  bool const swapped = SwapBag0Slots(characterGuid, srcSlot, *dstOpt);
  LOG_DEBUG(
      "AutoEquipFromBag0Slot: guid={} entry={} src={} dst={} swapped={}",
      characterGuid, entry, srcSlot, *dstOpt, swapped);
  return swapped;
}

bool MySqlCharacterRepository::DestroyBag0BackpackItem(uint32_t characterGuid,
                                                        uint8_t slot,
                                                        uint32_t clientCount) {
  if (slot < INVENTORY_SLOT_ITEM_START || slot >= INVENTORY_SLOT_ITEM_END) {
    LOG_DEBUG("DestroyBag0BackpackItem: guid={} invalid slot={}", characterGuid,
              slot);
    return false;
  }
  if (!EnsureStarterInventoryTables(_connection))
    return false;

  try {
    _connection->setAutoCommit(false);

    uint32_t itemGuid = 0;
    uint32_t stack = 0;
    {
      std::shared_ptr<sql::PreparedStatement> q(_connection->prepareStatement(
          "SELECT ci.item, ii.count FROM character_inventory ci "
          "INNER JOIN item_instance ii ON ii.guid = ci.item "
          "WHERE ci.guid = ? AND ci.bag = 0 AND ci.slot = ? LIMIT 1"));
      q->setUInt(1, characterGuid);
      q->setUInt(2, slot);
      std::unique_ptr<sql::ResultSet> rs(q->executeQuery());
      if (!rs->next()) {
        _connection->rollback();
        _connection->setAutoCommit(true);
        return false;
      }
      itemGuid = rs->getUInt("item");
      stack = std::max(1u, static_cast<uint32_t>(rs->getUInt("count")));
    }

    uint32_t const remove =
        (clientCount == 0) ? stack : std::min(clientCount, stack);
    if (remove == 0) {
      _connection->rollback();
      _connection->setAutoCommit(true);
      return false;
    }

    if (remove >= stack) {
      {
        std::shared_ptr<sql::PreparedStatement> delInv(
            _connection->prepareStatement(
                "DELETE FROM character_inventory WHERE guid = ? AND bag = 0 "
                "AND slot = ?"));
        delInv->setUInt(1, characterGuid);
        delInv->setUInt(2, slot);
        if (delInv->executeUpdate() != 1) {
          _connection->rollback();
          _connection->setAutoCommit(true);
          return false;
        }
      }
      {
        std::shared_ptr<sql::PreparedStatement> delItem(
            _connection->prepareStatement(
                "DELETE FROM item_instance WHERE guid = ? AND owner_guid = ?"));
        delItem->setUInt(1, itemGuid);
        delItem->setUInt(2, characterGuid);
        if (delItem->executeUpdate() != 1) {
          _connection->rollback();
          _connection->setAutoCommit(true);
          return false;
        }
      }
    } else {
      uint32_t const newCount = stack - remove;
      std::shared_ptr<sql::PreparedStatement> upd(
          _connection->prepareStatement(
              "UPDATE item_instance SET count = ? WHERE guid = ? AND owner_guid = ?"));
      upd->setUInt(1, newCount);
      upd->setUInt(2, itemGuid);
      upd->setUInt(3, characterGuid);
      if (upd->executeUpdate() != 1) {
        _connection->rollback();
        _connection->setAutoCommit(true);
        return false;
      }
    }

    _connection->commit();
    _connection->setAutoCommit(true);
    LOG_DEBUG("DestroyBag0BackpackItem: guid={} slot={} removed={} of stack={}",
              characterGuid, slot, remove, stack);
    return true;
  } catch (sql::SQLException &e) {
    try {
      _connection->rollback();
      _connection->setAutoCommit(true);
    } catch (...) {
    }
    LOG_ERROR("DestroyBag0BackpackItem failed: {}", e.what());
    return false;
  }
}

uint32_t MySqlCharacterRepository::RemoveBag0ItemsByEntry(uint32_t characterGuid,
                                                          uint32_t itemEntry,
                                                          uint32_t wantRemove) {
  if (characterGuid == 0 || itemEntry == 0 || wantRemove == 0)
    return 0;
  if (!EnsureStarterInventoryTables(_connection))
    return 0;
  uint32_t removed = 0;
  while (removed < wantRemove) {
    uint8_t slot = 0;
    uint32_t stack = 0;
    bool found = false;
    try {
      std::shared_ptr<sql::PreparedStatement> q(_connection->prepareStatement(
          "SELECT ci.slot, ii.count FROM character_inventory ci "
          "INNER JOIN item_instance ii ON ii.guid = ci.item "
          "WHERE ci.guid = ? AND ci.bag = 0 AND ci.slot >= ? AND ci.slot < ? "
          "AND ii.itemEntry = ? ORDER BY ci.slot LIMIT 1"));
      q->setUInt(1, characterGuid);
      q->setUInt(2, INVENTORY_SLOT_ITEM_START);
      q->setUInt(3, INVENTORY_SLOT_ITEM_END);
      q->setUInt(4, itemEntry);
      std::unique_ptr<sql::ResultSet> rs(q->executeQuery());
      if (!rs->next())
        break;
      slot = static_cast<uint8_t>(rs->getUInt("slot"));
      stack = std::max(1u, static_cast<uint32_t>(rs->getUInt("count")));
      found = true;
    } catch (sql::SQLException const &e) {
      LOG_WARN("RemoveBag0ItemsByEntry query failed: {}", e.what());
      break;
    }
    if (!found)
      break;
    uint32_t const take = std::min(stack, wantRemove - removed);
    if (!DestroyBag0BackpackItem(characterGuid, slot, take))
      break;
    removed += take;
  }
  return removed;
}

bool MySqlCharacterRepository::SwapBag0Slots(uint32_t characterGuid, uint8_t srcSlot,
                                               uint8_t dstSlot) {
  if (srcSlot == dstSlot)
    return true;
  try {
    _connection->setAutoCommit(false);

    bool hasSrc = false;
    bool hasDst = false;
    std::unordered_set<unsigned> usedSlots;
    {
      std::shared_ptr<sql::PreparedStatement> q(_connection->prepareStatement(
          "SELECT slot FROM character_inventory WHERE guid = ? AND bag = 0"));
      q->setUInt(1, characterGuid);
      std::unique_ptr<sql::ResultSet> rs(q->executeQuery());
      while (rs->next()) {
        unsigned const s = rs->getUInt("slot");
        usedSlots.insert(s);
        if (s == srcSlot)
          hasSrc = true;
        if (s == dstSlot)
          hasDst = true;
      }
    }

    if (!hasSrc) {
      LOG_WARN("SwapBag0Slots: source slot missing guid={} src={} dst={}",
               characterGuid, srcSlot, dstSlot);
      _connection->rollback();
      _connection->setAutoCommit(true);
      return false;
    }

    if (!hasDst) {
      std::shared_ptr<sql::PreparedStatement> move(_connection->prepareStatement(
          "UPDATE character_inventory SET slot = ? WHERE guid = ? AND bag = 0 "
          "AND slot = ?"));
      move->setUInt(1, dstSlot);
      move->setUInt(2, characterGuid);
      move->setUInt(3, srcSlot);
      if (move->executeUpdate() != 1) {
        _connection->rollback();
        _connection->setAutoCommit(true);
        return false;
      }
      _connection->commit();
      _connection->setAutoCommit(true);
      return true;
    }

    // Avoid UNIQUE(guid, bag, slot) conflicts by using a temporary free slot.
    unsigned tempSlot = 255u;
    while (tempSlot > 200u && usedSlots.count(tempSlot))
      --tempSlot;
    if (usedSlots.count(tempSlot)) {
      LOG_ERROR(
          "SwapBag0Slots: no temporary slot available guid={} src={} dst={}",
          characterGuid, srcSlot, dstSlot);
      _connection->rollback();
      _connection->setAutoCommit(true);
      return false;
    }

    {
      std::shared_ptr<sql::PreparedStatement> step1(_connection->prepareStatement(
          "UPDATE character_inventory SET slot = ? WHERE guid = ? AND bag = 0 "
          "AND slot = ?"));
      step1->setUInt(1, tempSlot);
      step1->setUInt(2, characterGuid);
      step1->setUInt(3, srcSlot);
      if (step1->executeUpdate() != 1) {
        _connection->rollback();
        _connection->setAutoCommit(true);
        return false;
      }
    }
    {
      std::shared_ptr<sql::PreparedStatement> step2(_connection->prepareStatement(
          "UPDATE character_inventory SET slot = ? WHERE guid = ? AND bag = 0 "
          "AND slot = ?"));
      step2->setUInt(1, srcSlot);
      step2->setUInt(2, characterGuid);
      step2->setUInt(3, dstSlot);
      if (step2->executeUpdate() != 1) {
        _connection->rollback();
        _connection->setAutoCommit(true);
        return false;
      }
    }
    {
      std::shared_ptr<sql::PreparedStatement> step3(_connection->prepareStatement(
          "UPDATE character_inventory SET slot = ? WHERE guid = ? AND bag = 0 "
          "AND slot = ?"));
      step3->setUInt(1, dstSlot);
      step3->setUInt(2, characterGuid);
      step3->setUInt(3, tempSlot);
      if (step3->executeUpdate() != 1) {
        _connection->rollback();
        _connection->setAutoCommit(true);
        return false;
      }
    }

    _connection->commit();
    _connection->setAutoCommit(true);
    return true;
  } catch (sql::SQLException &e) {
    try {
      _connection->rollback();
      _connection->setAutoCommit(true);
    } catch (...) {
    }
    LOG_ERROR("SwapBag0Slots failed: {}", e.what());
    return false;
  }
}

AccessLevel
MySqlCharacterRepository::GetAccountAccessLevel(uint32_t accountId) {
  try {
    std::shared_ptr<sql::PreparedStatement> st(
        _connection->prepareStatement(
            "SELECT access_level FROM firelands_auth.account WHERE id = ? LIMIT 1"));
    st->setUInt(1, accountId);
    std::unique_ptr<sql::ResultSet> rs(st->executeQuery());
    if (!rs->next())
      return AccessLevel::Player;
    return AccessLevelFromStored(
        static_cast<uint8_t>(rs->getUInt("access_level")));
  } catch (sql::SQLException &e) {
    LOG_WARN("GetAccountAccessLevel failed for account {}: {}", accountId,
             e.what());
    return AccessLevel::Player;
  }
}

bool MySqlCharacterRepository::SaveInventory(uint32_t characterGuid,
                                             Bag0InventoryData const &invData) {
  return SaveInventoryToDb(_connection, characterGuid, invData);
}

} // namespace Firelands
