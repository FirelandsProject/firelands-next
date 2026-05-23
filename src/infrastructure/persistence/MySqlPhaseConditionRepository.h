#pragma once

#include <domain/repositories/IPhaseConditionRepository.h>

#include <conncpp.hpp>
#include <memory>

namespace Firelands {

class MySqlPhaseConditionRepository : public IPhaseConditionRepository {
public:
  explicit MySqlPhaseConditionRepository(std::shared_ptr<sql::Connection> connection);

  PhaseConditionMap<PhaseConditionList> LoadPhaseConditions() const override;

private:
  std::shared_ptr<sql::Connection> m_connection;
};

} // namespace Firelands
