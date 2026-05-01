#include "MySqlRealmRepository.h"
#include <stdexcept>

namespace Firelands {

MySqlRealmRepository::MySqlRealmRepository(
    std::shared_ptr<sql::Connection> connection)
    : _connection(std::move(connection)) {
  if (!_connection) {
    throw std::invalid_argument("Database connection cannot be null");
  }
}

bool MySqlRealmRepository::FindById(uint32_t id) {
  std::unique_ptr<sql::PreparedStatement> pstmt(
      _connection->prepareStatement("SELECT id FROM realmlist WHERE id = ?"));
  pstmt->setUInt(1, id);
  std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
  return res->next();
}

void MySqlRealmRepository::DeleteById(uint32_t id) {
  std::unique_ptr<sql::PreparedStatement> pstmt(
      _connection->prepareStatement("DELETE FROM realmlist WHERE id = ?"));
  pstmt->setUInt(1, id);
  pstmt->execute();
}

void MySqlRealmRepository::Create(const Realm &realm) {
  std::unique_ptr<sql::PreparedStatement> pstmt(_connection->prepareStatement(
      "INSERT INTO realmlist (id, name, address, port, icon, timezone, "
      "allowedSecurityLevel, population) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
  pstmt->setUInt(1, realm.GetId());
  pstmt->setString(2, realm.GetName());
  pstmt->setString(3, realm.GetAddress());
  pstmt->setUInt(4, realm.GetPort());
  pstmt->setUInt(5, realm.GetIcon());
  pstmt->setUInt(6, realm.GetTimezone());
  pstmt->setUInt(7, realm.GetAllowedSecurityLevel());
  pstmt->setDouble(8, static_cast<double>(realm.GetPopulation()));
  pstmt->execute();
}

std::vector<Realm> MySqlRealmRepository::GetRealms() {
  std::vector<Realm> realms;

  try {
    std::unique_ptr<sql::Statement> stmt(_connection->createStatement());
    std::unique_ptr<sql::ResultSet> res(
        stmt->executeQuery("SELECT id, name, address, port, icon, timezone, "
                           "allowedSecurityLevel, population FROM realmlist"));

    while (res->next()) {
      uint32_t id = res->getUInt("id");
      std::string name = res->getString("name").c_str();
      std::string address = res->getString("address").c_str();
      uint16_t port = static_cast<uint16_t>(res->getUInt("port"));
      uint8_t icon = static_cast<uint8_t>(res->getUInt("icon"));
      uint8_t timezone = static_cast<uint8_t>(res->getUInt("timezone"));
      uint8_t secLevel =
          static_cast<uint8_t>(res->getUInt("allowedSecurityLevel"));
      float population = static_cast<float>(res->getDouble("population"));

      realms.emplace_back(id, name, address, port, icon, timezone, secLevel,
                          population, 0);
    }
  } catch (sql::SQLException &e) {
    // Implement proper logging using the existing Logger if necessary
    // For now we could rethrow or return empty or partially fetched list
    // Rethrowing an exception is more reliable for catching DB connectivity
    // errors early.
    throw std::runtime_error(
        std::string("SQLException in MySqlRealmRepository::GetRealms: ") +
        e.what());
  }

  return realms;
}

} // namespace Firelands
