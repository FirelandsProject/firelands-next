#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <application/services/CharacterService.h>
#include <domain/models/PlayerCreateInfo.h>
#include <domain/repositories/ICharacterRepository.h>

using namespace Firelands;
using namespace testing;

class MockCharacterRepository : public ICharacterRepository {
public:
    MOCK_METHOD(std::vector<std::shared_ptr<Character>>, GetCharactersByAccount, (uint32_t), (override));
    MOCK_METHOD(std::optional<uint32_t>, CreateCharacter, (const Character&), (override));
    MOCK_METHOD(bool, GrantStarterItems, (uint32_t, std::vector<StarterItemGrant> const&), (override));
    MOCK_METHOD(bool, DeleteCharacter, (uint32_t, uint32_t), (override));
    MOCK_METHOD(bool, IsNameAvailable, (const std::string&), (override));
    MOCK_METHOD(std::optional<Character>, GetCharacterByGuid, (uint64_t), (override));
    MOCK_METHOD(bool, SwapBag0Slots, (uint32_t, uint8_t, uint8_t), (override));
    MOCK_METHOD(bool, SaveCharacterOnLogout,
                (uint32_t, uint32_t, uint16_t, uint16_t, float, float, float,
                 float, uint32_t),
                (override));
    MOCK_METHOD(bool, UpdateCharacterMoney,
                (uint32_t, uint32_t, uint32_t), (override));
    MOCK_METHOD(bool, UpdateCharacterLevel,
                (uint32_t, uint32_t, uint8_t), (override));
    MOCK_METHOD(std::vector<uint32_t>, GetCharacterSpellIds, (uint32_t),
                (override));
    MOCK_METHOD(bool, AddCharacterSpell, (uint32_t, uint32_t), (override));
    MOCK_METHOD(bool, GrantItemToBag0, (uint32_t, uint32_t, uint32_t),
                (override));
    MOCK_METHOD(AccessLevel, GetAccountAccessLevel, (uint32_t), (override));
    MOCK_METHOD(bool, AutoEquipFromBag0Slot, (uint32_t, uint8_t), (override));
};

TEST(CharacterServiceTests, GetCharacters_ReturnsCharactersFromRepository) {
    auto repo = std::make_shared<MockCharacterRepository>();
    CharacterService service(repo);

    uint32_t accountId = 1;
    auto mockChar = std::make_shared<Character>(1, accountId, "Test", 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, true);
    std::vector<std::shared_ptr<Character>> characters = { mockChar };

    EXPECT_CALL(*repo, GetCharactersByAccount(accountId))
        .WillOnce(Return(characters));

    auto result = service.GetCharactersForAccount(accountId);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->GetName(), "Test");
}
