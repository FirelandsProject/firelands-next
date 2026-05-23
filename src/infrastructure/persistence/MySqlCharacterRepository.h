#ifndef FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_CHARACTER_REPOSITORY_H
#define FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_CHARACTER_REPOSITORY_H

#include <domain/repositories/ICharacterRepository.h>
#include <domain/models/PlayerCreateInfo.h>
#include <shared/Common.h>
#include <shared/dbc/CharStartOutfitDbc.h>
#include <shared/dbc/ChrRacesDbc.h>
#include <shared/dbc/ItemDb2Wdb2.h>
#include <conncpp.hpp>
#include <array>
#include <memory>
#include <optional>

namespace Firelands {

    class MySqlCharacterRepository : public ICharacterRepository {
    public:
        /// `characterConnection` — `firelands_characters`. `worldConnection` — optional
        /// `firelands_world` for `item_template` (recommended; falls back to character DB).
        explicit MySqlCharacterRepository(
            std::shared_ptr<sql::Connection> characterConnection,
            std::shared_ptr<sql::Connection> worldConnection = nullptr);

        std::vector<std::shared_ptr<Character>> GetCharactersByAccount(uint32_t accountId) override;
        std::optional<uint32_t> CreateCharacter(const Character& character) override;
        bool GrantStarterItems(uint32_t characterGuid, std::vector<StarterItemGrant> const& items) override;
        bool DeleteCharacter(uint32_t guid, uint32_t accountId) override;
        bool IsNameAvailable(const std::string& name) override;
        std::optional<Character> GetCharacterByGuid(uint64_t guid) override;
        bool SwapBag0Slots(uint32_t characterGuid, uint8_t srcSlot,
                           uint8_t dstSlot) override;
        bool SaveCharacterOnLogout(
            uint32_t accountId, uint32_t characterGuid, uint16_t mapId,
            uint16_t zoneId, float x, float y, float z, float orientation,
            uint32_t moneyCopper, uint32_t xp, float restBonus,
            std::array<uint32_t, Character::kTutorialMaskInts> const &tutorialMask,
            std::optional<uint32_t> liveHealth = std::nullopt,
            std::optional<uint32_t> livePower1 = std::nullopt) override;
        bool UpdateCharacterMoney(uint32_t accountId, uint32_t characterGuid,
                                  uint32_t moneyCopper) override;
        bool UpdateCharacterLevel(uint32_t accountId, uint32_t characterGuid,
                                   uint8_t level) override;
        bool UpdateCharacterLevelAndXp(uint32_t accountId, uint32_t characterGuid,
                                        uint8_t level, uint32_t xp,
                                        float restBonus) override;
        std::vector<uint32_t> GetCharacterSpellIds(uint32_t characterGuid) override;
        CharacterCooldownState LoadCharacterCooldowns(uint32_t characterGuid) override;
        bool SaveCharacterCooldowns(uint32_t characterGuid,
                                    CharacterCooldownState const &state) override;
        CharacterActionButtonState LoadCharacterActionButtons(uint32_t characterGuid,
                                                              uint8_t spec = 0) override;
        bool SaveCharacterActionButtons(uint32_t characterGuid, uint8_t spec,
                                        CharacterActionButtonState const &state) override;
        bool UpsertCharacterActionButton(uint32_t characterGuid, uint8_t spec,
                                         uint8_t button, uint32_t action,
                                         uint8_t type) override;
        bool DeleteCharacterActionButton(uint32_t characterGuid, uint8_t spec,
                                         uint8_t button) override;
        bool UpdateCharacterActionBarToggles(uint32_t characterGuid,
                                             uint8_t toggles) override;
        bool AddCharacterSpell(uint32_t characterGuid, uint32_t spellId) override;
        bool RemoveCharacterSpell(uint32_t characterGuid, uint32_t spellId) override;
        bool HasItemTemplate(uint32_t itemEntry) const override;
        bool GrantItemToBag0(uint32_t characterGuid, uint32_t itemEntry, uint32_t count,
                             uint32_t *outItemGuidLow = nullptr,
                             uint8_t *outBag0Slot = nullptr) override;
        bool SendGmMailWithItem(uint32_t receiverCharacterGuid, uint32_t itemEntry,
                                uint32_t count) override;
        uint32_t RemoveBag0ItemsByEntry(uint32_t characterGuid, uint32_t itemEntry,
                                       uint32_t count) override;
        AccessLevel GetAccountAccessLevel(uint32_t accountId) override;
        bool AutoEquipFromBag0Slot(
            uint32_t characterGuid, uint8_t srcSlot,
            std::optional<uint8_t> fallbackInventoryType = std::nullopt) override;
        bool DestroyBag0BackpackItem(uint32_t characterGuid, uint8_t slot,
                                     uint32_t clientCount) override;
        bool SaveInventory(uint32_t characterGuid,
                           Bag0InventoryData const &invData) override;
        std::vector<MailInboxRow> LoadMailInbox(uint32_t receiverGuid) override;

    private:
        void ApplyInitialFactionTemplate(Character &character, uint8_t race) const;

        std::shared_ptr<sql::Connection> _connection;
        std::shared_ptr<sql::Connection> _worldConnection;
        CharStartOutfitDbc _charStartOutfitDbc;
        ChrRacesDbc _chrRacesDbc;
        bool _charStartOutfitLoaded = false;
        ItemDb2Wdb2 _itemDb2;
        std::shared_ptr<sql::Connection> itemTemplateConnection() const {
            return _worldConnection ? _worldConnection : _connection;
        }
    };

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_PERSISTENCE_MYSQL_CHARACTER_REPOSITORY_H
