#pragma once

#include <domain/repositories/IPlayerQuestProgressRepository.h>

#include <conncpp.hpp>
#include <memory>

namespace Firelands {

class MySqlPlayerQuestProgressRepository : public IPlayerQuestProgressRepository {
public:
  explicit MySqlPlayerQuestProgressRepository(std::shared_ptr<sql::Connection> connection);

  PlayerQuestProgressSnapshot LoadForCharacter(uint32 characterGuid) const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
