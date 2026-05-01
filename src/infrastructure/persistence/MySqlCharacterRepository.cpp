#include "MySqlCharacterRepository.h"
#include <domain/models/PlayerCreateInfo.h>
#include <shared/game/InventorySlots.h>
#include <shared/game/ItemEquipSlots.h>
#include <shared/Logger.h>
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <unordered_set>

namespace Firelands {

namespace {

struct ItemProtoRow {
  uint8 inventoryType = 0;
  uint32 buyCount = 1;
};

struct EquippedItemsData {
  std::array<uint32_t, kEquipmentSlotCount> entries{};
  std::array<uint32_t, kEquipmentSlotCount> guids{};
};

bool IsMissingTableError(sql::SQLException &e) {
  return e.getErrorCode() == 1146 || e.getSQLState() == "42S02";
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

EquippedItemsData LoadEquippedItems(std::shared_ptr<sql::Connection> conn,
                                    uint32_t charGuid) {
  EquippedItemsData out;
  if (!EnsureStarterInventoryTables(conn))
    return out;
  try {
    std::shared_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "SELECT ci.slot, ci.item, ii.itemEntry FROM character_inventory ci "
        "INNER JOIN item_instance ii ON ii.guid = ci.item "
        "WHERE ci.guid = ? AND ci.bag = 0 AND ci.slot < ?"));
    ps->setUInt(1, charGuid);
    ps->setUInt(2, static_cast<unsigned>(EQUIPMENT_SLOT_END));
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    while (rs->next()) {
      unsigned slot = rs->getUInt("slot");
      if (slot >= kEquipmentSlotCount)
        continue;
      out.entries[slot] = rs->getUInt("itemEntry");
      out.guids[slot] = rs->getUInt("item");
    }
  } catch (sql::SQLException const &e) {
    LOG_WARN("LoadEquippedItems failed for guid {}: {}", charGuid,
             e.what());
  }
  return out;
}

} // namespace

MySqlCharacterRepository::MySqlCharacterRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {}

std::vector<std::shared_ptr<Character>>
MySqlCharacterRepository::GetCharactersByAccount(uint32_t accountId) {
  std::vector<std::shared_ptr<Character>> characters;
  try {
    std::shared_ptr<sql::PreparedStatement> stmnt(_connection->prepareStatement(
        "SELECT guid, account, name, race, class, gender, skin, face, "
        "hairStyle, hairColor, facialHair, outfitId, equipmentCache, "
        "level, zoneId, mapId, x, y, z, guildId, characterFlags, "
        "customizationFlags, firstLogin "
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
          static_cast<uint16>(res->getUInt("mapId")), res->getFloat("x"),
          res->getFloat("y"), res->getFloat("z"),
          0.0f, // orientation (pending DB update)
          res->getUInt("guildId"), res->getUInt("characterFlags"),
          res->getUInt("customizationFlags"), res->getBoolean("firstLogin"),
          static_cast<uint8>(res->getUInt("outfitId")),
          res->isNull("equipmentCache") ? ""
                                        : std::string(res->getString("equipmentCache")),
          std::array<uint32_t, kEquipmentSlotCount>{},
          std::array<uint32_t, kEquipmentSlotCount>{}));
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
        "level, zoneId, mapId, x, y, z, guildId, characterFlags, "
        "customizationFlags, firstLogin) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

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
    stmnt->setUInt(19, character.GetGuildId());
    stmnt->setUInt(20, character.GetCharacterFlags());
    stmnt->setUInt(21, character.GetCustomizationFlags());
    stmnt->setBoolean(22, character.IsFirstLogin());

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
        "level, zoneId, mapId, x, y, z, guildId, characterFlags, "
        "customizationFlags, firstLogin "
        "FROM characters WHERE guid = ?"));
    stmnt->setUInt64(1, guid);

    std::unique_ptr<sql::ResultSet> res(stmnt->executeQuery());

    if (res->next()) {
      uint32_t lowGuid = res->getUInt("guid");
      auto equipped = LoadEquippedItems(_connection, lowGuid);
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
          static_cast<uint16>(res->getUInt("mapId")), res->getFloat("x"),
          res->getFloat("y"), res->getFloat("z"),
          0.0f, // orientation (pending DB update)
          res->getUInt("guildId"), res->getUInt("characterFlags"),
          res->getUInt("customizationFlags"), res->getBoolean("firstLogin"),
          static_cast<uint8>(res->getUInt("outfitId")),
          res->isNull("equipmentCache") ? ""
                                        : std::string(res->getString("equipmentCache")),
          equipped.entries, equipped.guids);
    }
    return std::nullopt;
  } catch (sql::SQLException &e) {
    LOG_ERROR("SQLException in GetCharacterByGuid: {}", e.what());
    return std::nullopt;
  }
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

} // namespace Firelands
