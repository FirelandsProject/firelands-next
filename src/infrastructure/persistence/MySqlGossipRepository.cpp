#include "MySqlGossipRepository.h"
#include <shared/Logger.h>
#include <unordered_map>

namespace Firelands {

namespace {

std::string SafeSqlString(std::optional<sql::SQLString> const &val) {
  if (!val.has_value())
    return "";
  return std::string(val->c_str());
}

using OptionKey = uint64_t;

OptionKey MakeOptionKey(uint32_t menuId, uint32_t optionIndex) {
  return (static_cast<uint64_t>(menuId) << 32) | optionIndex;
}

struct GossipOptionBoxRow {
  bool isCoded = false;
  uint32_t boxMoney = 0;
  std::string boxMessage;
  uint32_t boxBroadcastTextId = 0;
};

struct GossipOptionActionRow {
  uint32_t actionMenuId = 0;
  uint32_t actionPoi = 0;
};

} // namespace

MySqlGossipRepository::MySqlGossipRepository(std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {}

std::optional<uint32_t> MySqlGossipRepository::GetMenuTextId(uint32_t menuId) const {
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(
        _connection->prepareStatement(
            "SELECT `TextID` FROM `gossip_menu` WHERE `MenuID` = ? "
            "ORDER BY `TextID` LIMIT 1"));
    stmt->setUInt(1, menuId);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    if (rs->next())
      return rs->getUInt("TextID");
    return std::nullopt;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("MySqlGossipRepository::GetMenuTextId failed for menuId={}: {}", menuId,
              e.what());
    return std::nullopt;
  }
}

std::vector<GossipMenuItem> MySqlGossipRepository::GetMenuOptions(uint32_t menuId) const {
  std::vector<GossipMenuItem> result;
  try {
    std::unordered_map<OptionKey, GossipOptionBoxRow> boxes;
    std::unordered_map<OptionKey, GossipOptionActionRow> actions;

    {
      std::unique_ptr<sql::PreparedStatement> boxStmt(_connection->prepareStatement(
          "SELECT `OptionIndex`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextId` "
          "FROM `gossip_menu_option_box` WHERE `MenuId` = ?"));
      boxStmt->setUInt(1, menuId);
      std::unique_ptr<sql::ResultSet> boxRs(boxStmt->executeQuery());
      while (boxRs->next()) {
        auto const key = MakeOptionKey(menuId, boxRs->getUInt("OptionIndex"));
        boxes[key] = GossipOptionBoxRow{
            boxRs->getUInt("BoxCoded") != 0,
            boxRs->getUInt("BoxMoney"),
            SafeSqlString(boxRs->getString("BoxText")),
            boxRs->getUInt("BoxBroadcastTextId"),
        };
      }
    }

    {
      std::unique_ptr<sql::PreparedStatement> actStmt(_connection->prepareStatement(
          "SELECT `OptionIndex`, `ActionMenuId`, `ActionPoiId` "
          "FROM `gossip_menu_option_action` WHERE `MenuId` = ?"));
      actStmt->setUInt(1, menuId);
      std::unique_ptr<sql::ResultSet> actRs(actStmt->executeQuery());
      while (actRs->next()) {
        auto const key = MakeOptionKey(menuId, actRs->getUInt("OptionIndex"));
        actions[key] = GossipOptionActionRow{actRs->getUInt("ActionMenuId"),
                                             actRs->getUInt("ActionPoiId")};
      }
    }

    std::unique_ptr<sql::PreparedStatement> stmt(_connection->prepareStatement(
        "SELECT `MenuId`, `OptionIndex`, `OptionIcon`, `OptionText`, "
        "`OptionBroadcastTextId`, `OptionType`, `OptionNpcflag`, `VerifiedBuild` "
        "FROM `gossip_menu_option` WHERE `MenuId` = ? ORDER BY `OptionIndex`"));
    stmt->setUInt(1, menuId);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    while (rs->next()) {
      GossipMenuItem item;
      item.menuId = rs->getUInt("MenuId");
      item.optionIndex = rs->getUInt("OptionIndex");
      item.icon = static_cast<GossipOptionIcon>(rs->getUInt("OptionIcon"));
      item.optionText = SafeSqlString(rs->getString("OptionText"));
      item.optionBroadcastTextId = rs->getUInt("OptionBroadcastTextId");
      item.optionType = static_cast<GossipOptionType>(rs->getUInt("OptionType"));
      item.optionNpcflag = rs->getUInt64("OptionNpcflag");
      item.verifiedBuild = static_cast<uint16_t>(rs->getShort("VerifiedBuild"));

      auto const key = MakeOptionKey(item.menuId, item.optionIndex);
      if (auto const boxIt = boxes.find(key); boxIt != boxes.end()) {
        item.isCoded = boxIt->second.isCoded;
        item.boxMoney = boxIt->second.boxMoney;
        item.boxMessage = boxIt->second.boxMessage;
        item.boxBroadcastTextId = boxIt->second.boxBroadcastTextId;
      }
      if (auto const actIt = actions.find(key); actIt != actions.end()) {
        item.actionMenuId = actIt->second.actionMenuId;
        item.actionPoi = actIt->second.actionPoi;
      }

      result.push_back(std::move(item));
    }
  } catch (sql::SQLException const &e) {
    LOG_ERROR("MySqlGossipRepository::GetMenuOptions failed for menuId={}: {}", menuId,
              e.what());
  }
  return result;
}

} // namespace Firelands
