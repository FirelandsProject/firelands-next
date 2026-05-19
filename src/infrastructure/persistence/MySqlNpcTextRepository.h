#pragma once

#include <domain/repositories/INpcTextRepository.h>
#include <conncpp.hpp>
#include <memory>

namespace Firelands {

class MySqlNpcTextRepository final : public INpcTextRepository {
public:
  explicit MySqlNpcTextRepository(std::shared_ptr<sql::Connection> connection);

  std::optional<NpcText> TryGetById(uint32_t textId) const override;

private:
  std::shared_ptr<sql::Connection> _connection;
};

} // namespace Firelands
