#pragma once

#include <domain/models/Character.h>
#include <domain/models/PlayerCreateInfo.h>
#include <memory>
#include <optional>
#include <vector>

namespace Firelands {

class ICharacterRepository {
public:
  virtual ~ICharacterRepository() = default;

  virtual std::vector<std::shared_ptr<Character>>
  GetCharactersByAccount(uint32_t accountId) = 0;
  virtual std::optional<uint32_t> CreateCharacter(const Character &character) = 0;
  virtual bool GrantStarterItems(uint32_t characterGuid,
                                 std::vector<StarterItemGrant> const &items) = 0;
  virtual bool DeleteCharacter(uint32_t guid, uint32_t accountId) = 0;
  virtual bool IsNameAvailable(const std::string &name) = 0;
  virtual std::optional<Character> GetCharacterByGuid(uint64_t guid) = 0;
};

} // namespace Firelands
