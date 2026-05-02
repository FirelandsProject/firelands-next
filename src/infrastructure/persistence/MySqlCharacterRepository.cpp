#include "MySqlCharacterRepository.h"
#include <domain/models/PlayerCreateInfo.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/ItemEquipSlots.h>
#include <shared/Logger.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace Firelands {

namespace {

struct ItemProtoRow {
  uint8 inventoryType = 0;
  uint32 buyCount = 1;
};

struct Bag0InventoryData {
  std::array<uint32_t, kEquipmentSlotCount> equipEntries{};
  std::array<uint32_t, kEquipmentSlotCount> equipGuids{};
  std::array<uint32_t, kEquipmentSlotCount> equipStacks{};
  std::array<uint32_t, kPackSlotCount> packEntries{};
  std::array<uint32_t, kPackSlotCount> packGuids{};
  std::array<uint32_t, kPackSlotCount> packStacks{};
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
    LOG_INFO(
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
    LOG_INFO("Added missing column `firelands_characters.characters.money`.");
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
        "enchantments TEXT NOT NULL,"
        "randomPropertyType TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        "randomPropertyId INT UNSIGNED NOT NULL DEFAULT 0,"
        "durability SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        "creationTime INT UNSIGNED NOT NULL DEFAULT 0,"
        "text TEXT,"
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

std::optional<ItemProtoRow> FetchItemProto(std::shared_ptr<sql::Connection> conn,
                                            uint32_t itemEntry) {
  try {
    {
      std::shared_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
          "SELECT InventoryType, buy_count FROM "
          "firelands_world.item_proto_cache WHERE entry = ? LIMIT 1"));
      ps->setUInt(1, itemEntry);
      std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
      if (rs->next()) {
        ItemProtoRow row;
        row.inventoryType = static_cast<uint8>(rs->getUInt("InventoryType"));
        row.buyCount =
            std::max(1u, static_cast<uint32_t>(rs->getInt("buy_count")));
        return row;
      }
    }
    std::shared_ptr<sql::PreparedStatement> ps2(conn->prepareStatement(
        "SELECT InventoryType, BuyCount FROM firelands_world.item_template "
        "WHERE entry = ? LIMIT 1"));
    ps2->setUInt(1, itemEntry);
    std::unique_ptr<sql::ResultSet> rs2(ps2->executeQuery());
    if (rs2->next()) {
      ItemProtoRow row;
      row.inventoryType = static_cast<uint8>(rs2->getUInt("InventoryType"));
      row.buyCount =
          std::max(1u, static_cast<uint32_t>(rs2->getInt("BuyCount")));
      return row;
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("FetchItemProto failed for entry {}: {}", itemEntry, e.what());
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

} // namespace

MySqlCharacterRepository::MySqlCharacterRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {
  EnsureCharactersOrientationColumn(_connection);
  EnsureCharactersMoneyColumn(_connection);
  EnsureCharacterSpellTable(_connection);
}

std::vector<std::shared_ptr<Character>>
MySqlCharacterRepository::GetCharactersByAccount(uint32_t accountId) {
  std::vector<std::shared_ptr<Character>> characters;
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "SELECT guid, account, name, race, class, gender, skin, face, "
        "hairStyle, hairColor, facialHair, outfitId, equipmentCache, "
        "level, zoneId, mapId, x, y, z, orientation, guildId, characterFlags, "
        "customizationFlags, firstLogin, money "
        "FROM characters WHERE account = ?"));
    stmnt->setUInt(1, accountId);

    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

    while (res->next()) {
      characters.push_back(std::make_shared<Character>(
          res->getUInt("guid"), res->getUInt("account"),
          std::string(res->getString("name")),
          static_cast<uint8>(res->getUInt("race")),
          static_cast<uint8>(res->getUInt("class")),
          static_cast<uint8>(res->getUInt("gender")),
          static_cast<uint8>(res->getUInt("skin")),
          static_cast<uint8>(res->getUInt("face")),
          static_cast<uint8>(res->getUInt("hairStyle")),
          static_cast<uint8>(res->getUInt("hairColor")),
          static_cast<uint8>(res->getUInt("facialHair")),
          static_cast<uint8>(res->getUInt("level")),
          static_cast<uint16>(res->getUInt("zoneId")),
          static_cast<uint16>(res->getUInt("mapId")),
          ResultSetWorldFloat(*res, "x"), ResultSetWorldFloat(*res, "y"),
          ResultSetWorldFloat(*res, "z"), ResultSetWorldFloat(*res, "orientation"),
          res->getUInt("guildId"), res->getUInt("characterFlags"),
          res->getUInt("customizationFlags"), res->getBoolean("firstLogin"),
          static_cast<uint8>(res->getUInt("outfitId")),
          res->isNull("equipmentCache") ? ""
                                        : std::string(res->getString("equipmentCache")),
          std::array<uint32_t, kEquipmentSlotCount>{},
          std::array<uint32_t, kEquipmentSlotCount>{},
          std::array<uint32_t, kEquipmentSlotCount>{},
          std::array<uint32_t, kPackSlotCount>{},
          std::array<uint32_t, kPackSlotCount>{},
          std::array<uint32_t, kPackSlotCount>{},
          res->getUInt("money")));
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

      auto proto = FetchItemProto(_connection, grant.itemId);
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

      if (auto equipSlot = equipAllocator.TryEquipSlot(inventoryType)) {
        slot = *equipSlot;
        placed = true;
      } else {
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
      uint32_t lowGuid = res->getUInt("guid");
      auto bag0 = LoadBag0Inventory(_connection, lowGuid);
      return Character(
          lowGuid, res->getUInt("account"),
          std::string(res->getString("name")),
          static_cast<uint8>(res->getUInt("race")),
          static_cast<uint8>(res->getUInt("class")),
          static_cast<uint8>(res->getUInt("gender")),
          static_cast<uint8>(res->getUInt("skin")),
          static_cast<uint8>(res->getUInt("face")),
          static_cast<uint8>(res->getUInt("hairStyle")),
          static_cast<uint8>(res->getUInt("hairColor")),
          static_cast<uint8>(res->getUInt("facialHair")),
          static_cast<uint8>(res->getUInt("level")),
          static_cast<uint16>(res->getUInt("zoneId")),
          static_cast<uint16>(res->getUInt("mapId")),
          ResultSetWorldFloat(*res, "x"), ResultSetWorldFloat(*res, "y"),
          ResultSetWorldFloat(*res, "z"), ResultSetWorldFloat(*res, "orientation"),
          res->getUInt("guildId"), res->getUInt("characterFlags"),
          res->getUInt("customizationFlags"), res->getBoolean("firstLogin"),
          static_cast<uint8>(res->getUInt("outfitId")),
          res->isNull("equipmentCache") ? ""
                                        : std::string(res->getString("equipmentCache")),
          bag0.equipEntries, bag0.equipGuids, bag0.equipStacks,
          bag0.packEntries, bag0.packGuids, bag0.packStacks,
          res->getUInt("money"));
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

bool MySqlCharacterRepository::GrantItemToBag0(uint32_t characterGuid,
                                               uint32_t itemEntry,
                                               uint32_t count) {
  if (itemEntry == 0 || count == 0)
    return false;
  if (!EnsureStarterInventoryTables(_connection))
    return false;

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

  auto proto = FetchItemProto(_connection, itemEntry);
  uint32_t grantCount = count;
  if (proto) {
    if (grantCount == 0)
      grantCount = proto->buyCount;
  } else {
    grantCount = std::max(1u, grantCount);
    LOG_WARN("GrantItemToBag0: item {} missing template/proto.", itemEntry);
  }
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
    return true;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("GrantItemToBag0 failed: {}", e.what());
    return false;
  }
}

bool MySqlCharacterRepository::AutoEquipFromBag0Slot(uint32_t characterGuid,
                                                     uint8_t srcSlot) {
  if (srcSlot < INVENTORY_SLOT_ITEM_START || srcSlot >= INVENTORY_SLOT_ITEM_END) {
    LOG_INFO("AutoEquipFromBag0Slot: guid={} invalid srcSlot={}", characterGuid,
             srcSlot);
    return false;
  }
  auto bag0 = LoadBag0Inventory(_connection, characterGuid);
  size_t const pi = static_cast<size_t>(srcSlot - INVENTORY_SLOT_ITEM_START);
  uint32_t const entry = bag0.packEntries[pi];
  if (entry == 0) {
    LOG_INFO("AutoEquipFromBag0Slot: guid={} empty bag slot={}", characterGuid,
             srcSlot);
    return false;
  }
  auto proto = FetchItemProto(_connection, entry);
  if (!proto) {
    LOG_INFO("AutoEquipFromBag0Slot: guid={} entry={} missing item proto",
             characterGuid, entry);
    return false;
  }
  auto dstOpt = PrimaryEquipSlotForInventoryType(proto->inventoryType);
  if (!dstOpt) {
    LOG_INFO(
        "AutoEquipFromBag0Slot: guid={} entry={} inventoryType={} has no equip slot",
        characterGuid, entry, static_cast<uint32_t>(proto->inventoryType));
    return false;
  }
  bool const swapped = SwapBag0Slots(characterGuid, srcSlot, *dstOpt);
  LOG_INFO(
      "AutoEquipFromBag0Slot: guid={} entry={} src={} dst={} swapped={}",
      characterGuid, entry, srcSlot, *dstOpt, swapped);
  return swapped;
}

bool MySqlCharacterRepository::SwapBag0Slots(uint32_t characterGuid, uint8_t srcSlot,
                                               uint8_t dstSlot) {
  if (srcSlot == dstSlot)
    return true;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(_connection->prepareStatement(
        "UPDATE character_inventory SET slot = CASE WHEN slot = ? THEN ? WHEN "
        "slot = ? THEN ? END WHERE guid = ? AND bag = 0 AND slot IN (?, ?)"));
    ps->setUInt(1, srcSlot);
    ps->setUInt(2, dstSlot);
    ps->setUInt(3, dstSlot);
    ps->setUInt(4, srcSlot);
    ps->setUInt(5, characterGuid);
    ps->setUInt(6, srcSlot);
    ps->setUInt(7, dstSlot);
    ps->executeUpdate();
    return true;
  } catch (sql::SQLException &e) {
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

} // namespace Firelands
