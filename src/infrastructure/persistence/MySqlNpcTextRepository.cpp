#include "MySqlNpcTextRepository.h"
#include <shared/Logger.h>
#include <string>

namespace Firelands {

namespace {

std::string SafeSqlString(std::optional<sql::SQLString> const &val) {
  if (!val.has_value())
    return "";
  return std::string(val->c_str());
}

void LoadOptionFromRow(sql::ResultSet const &rs, std::size_t index,
                       NpcTextOption &out) {
  auto const prefix = std::to_string(index);

  out.text0 = SafeSqlString(rs.getString("text" + prefix + "_0"));
  out.text1 = SafeSqlString(rs.getString("text" + prefix + "_1"));
  out.language = static_cast<uint8_t>(rs.getUInt("lang" + prefix));
  out.probability = static_cast<float>(rs.getDouble("Probability" + prefix));

  for (std::size_t e = 0; e < kNpcTextEmoteCount; ++e) {
    auto const es = std::to_string(e);
    out.emotes[e].delay = static_cast<uint16_t>(
        rs.getUInt("EmoteDelay" + prefix + "_" + es));
    out.emotes[e].emote =
        static_cast<uint16_t>(rs.getUInt("Emote" + prefix + "_" + es));
  }
}

} // namespace

MySqlNpcTextRepository::MySqlNpcTextRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {}

std::optional<NpcText> MySqlNpcTextRepository::TryGetById(uint32_t textId) const {
  if (textId == 0)
    return std::nullopt;

  try {
    std::unique_ptr<sql::PreparedStatement> stmt(
        _connection->prepareStatement("SELECT * FROM `npc_text` WHERE `ID` = ? LIMIT 1"));
    stmt->setUInt(1, textId);
    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    if (!rs->next())
      return std::nullopt;

    NpcText text;
    text.id = rs->getUInt("ID");
    for (std::size_t i = 0; i < kNpcTextOptionCount; ++i)
      LoadOptionFromRow(*rs, i, text.options[i]);
    return text;
  } catch (sql::SQLException const &e) {
    LOG_ERROR("MySqlNpcTextRepository::TryGetById failed for textId={}: {}", textId,
              e.what());
    return std::nullopt;
  }
}

} // namespace Firelands
