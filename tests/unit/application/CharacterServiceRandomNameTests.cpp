#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <application/services/CharacterService.h>
#include <domain/repositories/ICharacterRepository.h>
#include <shared/dbc/NameGenDbc.h>

#include <string>

using namespace Firelands;
using namespace testing;

namespace {

std::string const kNameGenDbcPath =
    std::string(FIRELANDS_TEST_DATA_DIR) + "/data/dbc/NameGen.dbc";

class MockCharacterRepositoryMinimal : public ICharacterRepository {
public:
  MOCK_METHOD(std::vector<std::shared_ptr<Character>>, GetCharactersByAccount,
              (uint32_t), (override));
  MOCK_METHOD(std::optional<uint32_t>, CreateCharacter, (const Character &),
              (override));
  MOCK_METHOD(bool, GrantStarterItems, (uint32_t, std::vector<StarterItemGrant> const &),
              (override));
  MOCK_METHOD(bool, DeleteCharacter, (uint32_t, uint32_t), (override));
  MOCK_METHOD(bool, IsNameAvailable, (const std::string &), (override));
  MOCK_METHOD(std::optional<Character>, GetCharacterByGuid, (uint64_t), (override));
  MOCK_METHOD(bool, SwapBag0Slots, (uint32_t, uint8_t, uint8_t), (override));
  MOCK_METHOD(bool, SaveCharacterOnLogout,
              (uint32_t, uint32_t, uint16_t, uint16_t, float, float, float, float,
               uint32_t, uint32_t, float,
               (std::array<uint32_t, Character::kTutorialMaskInts> const &),
               std::optional<uint32_t>, std::optional<uint32_t>),
              (override));
  MOCK_METHOD(bool, UpdateCharacterMoney, (uint32_t, uint32_t, uint32_t), (override));
  MOCK_METHOD(bool, UpdateCharacterLevel, (uint32_t, uint32_t, uint8_t), (override));
  MOCK_METHOD(bool, UpdateCharacterLevelAndXp,
              (uint32_t, uint32_t, uint8_t, uint32_t, float), (override));
  MOCK_METHOD(std::vector<uint32_t>, GetCharacterSpellIds, (uint32_t), (override));
  MOCK_METHOD(CharacterCooldownState, LoadCharacterCooldowns, (uint32_t), (override));
  MOCK_METHOD(bool, SaveCharacterCooldowns, (uint32_t, CharacterCooldownState const &),
              (override));
  MOCK_METHOD(CharacterActionButtonState, LoadCharacterActionButtons, (uint32_t, uint8_t),
              (override));
  MOCK_METHOD(bool, SaveCharacterActionButtons,
              (uint32_t, uint8_t, CharacterActionButtonState const &), (override));
  MOCK_METHOD(bool, UpsertCharacterActionButton,
              (uint32_t, uint8_t, uint8_t, uint32_t, uint8_t), (override));
  MOCK_METHOD(bool, DeleteCharacterActionButton, (uint32_t, uint8_t, uint8_t),
              (override));
  MOCK_METHOD(bool, UpdateCharacterActionBarToggles, (uint32_t, uint8_t), (override));
  MOCK_METHOD(bool, AddCharacterSpell, (uint32_t, uint32_t), (override));
  MOCK_METHOD(bool, RemoveCharacterSpell, (uint32_t, uint32_t), (override));
  MOCK_METHOD(bool, HasItemTemplate, (uint32_t), (const, override));
  MOCK_METHOD(bool, GrantItemToBag0, (uint32_t, uint32_t, uint32_t, uint32_t *, uint8_t *),
              (override));
  MOCK_METHOD(bool, SendGmMailWithItem, (uint32_t, uint32_t, uint32_t), (override));
  MOCK_METHOD(uint32_t, RemoveBag0ItemsByEntry, (uint32_t, uint32_t, uint32_t),
              (override));
  MOCK_METHOD(bool, AutoEquipFromBag0Slot, (uint32_t, uint8_t, std::optional<uint8_t>),
              (override));
  MOCK_METHOD(bool, DestroyBag0BackpackItem, (uint32_t, uint8_t, uint32_t), (override));
  MOCK_METHOD(bool, SaveInventory, (uint32_t, Bag0InventoryData const &), (override));
  MOCK_METHOD(std::vector<MailInboxRow>, LoadMailInbox, (uint32_t), (override));
};

} // namespace

TEST(CharacterServiceRandomNameTests, WithoutNameGenDbc_ReturnsEmpty) {
  auto repo = std::make_shared<MockCharacterRepositoryMinimal>();
  CharacterService service(repo);
  EXPECT_FALSE(service.GenerateRandomCharacterName(1, 0).has_value());
}

TEST(CharacterServiceRandomNameTests, WithNameGenDbc_ReturnsGenderedName) {
  auto repo = std::make_shared<MockCharacterRepositoryMinimal>();
  auto nameGen = std::make_shared<NameGenDbc>();
  ASSERT_TRUE(nameGen->Load(kNameGenDbcPath));
  CharacterService service(repo, nullptr, nameGen);

  EXPECT_TRUE(service.GenerateRandomCharacterName(1, 0).has_value());
  EXPECT_TRUE(service.GenerateRandomCharacterName(1, 1).has_value());
  EXPECT_FALSE(service.GenerateRandomCharacterName(99, 0).has_value());
}
