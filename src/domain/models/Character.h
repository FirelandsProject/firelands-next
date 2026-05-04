#pragma once

#include <shared/Common.h>
#include <shared/game/Bag0InventoryData.h>
#include <shared/game/InventorySlots.h>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>

namespace Firelands {

class Character {
public:
  /// Words mirrored by `SMSG_TUTORIAL_FLAGS` (Trinity `MAX_ACCOUNT_TUTORIAL_VALUES`).
  static constexpr size_t kTutorialMaskInts = 8;

  Character(uint32 guid, uint32 account, std::string name, uint8 race,
            uint8 klass, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle,
            uint8 hairColor, uint8 facialHair, uint8 level, uint16 zoneId,
            uint16 mapId, float x, float y, float z, float o, uint32 guildId,
            uint32 characterFlags, uint32 customizationFlags, bool firstLogin,
            uint8 outfitId = 0, std::string equipmentCache = "",
            std::array<uint32_t, kEquipmentSlotCount> visibleItems = {},
            std::array<uint32_t, kEquipmentSlotCount> visibleItemGuids = {},
            std::array<uint32_t, kEquipmentSlotCount> visibleItemStacks = {},
            std::array<uint32_t, kPackSlotCount> packItemEntries = {},
            std::array<uint32_t, kPackSlotCount> packItemGuids = {},
            std::array<uint32_t, kPackSlotCount> packItemStacks = {},
            uint32 moneyCopper = 0, uint32_t xp = 0,
            std::array<uint32_t, kTutorialMaskInts> tutorialMask = {})
      : m_guid(guid), m_account(account), m_name(std::move(name)), m_race(race),
        m_klass(klass), m_gender(gender), m_skin(skin), m_face(face),
        m_hairStyle(hairStyle), m_hairColor(hairColor),
        m_facialHair(facialHair), m_level(level), m_zoneId(zoneId),
        m_mapId(mapId), m_x(x), m_y(y), m_z(z), m_o(o), m_guildId(guildId),
        m_characterFlags(characterFlags),
        m_customizationFlags(customizationFlags), m_firstLogin(firstLogin),
        m_outfitId(outfitId), m_equipmentCache(std::move(equipmentCache)),
        m_visibleItems(visibleItems), m_visibleItemGuids(visibleItemGuids),
        m_visibleItemStacks(visibleItemStacks), m_packItemEntries(packItemEntries),
        m_packItemGuids(packItemGuids), m_packItemStacks(packItemStacks),
        m_moneyCopper(moneyCopper), m_xp(xp), m_tutorialMask(tutorialMask),
        m_health(100), m_maxHealth(100),
        m_factionTemplate(1),
        m_displayId(GetDefaultDisplayId(race, gender)),
        m_primaryStats(GetDefaultPrimaryStats(klass)) {}

  /// `UNIT_FIELD_FACTIONTEMPLATE` from `ChrRaces.dbc` FactionID (or safe fallback).
  void SetInitialFactionTemplateFromServer(uint32 factionTemplate) {
    m_factionTemplate = factionTemplate;
  }

  static uint32 GetDefaultDisplayId(uint8 race, uint8 gender) {
    switch (race) {
    case 1:
      return gender == 0 ? 49 : 50; // Human
    case 2:
      return gender == 0 ? 51 : 52; // Orc
    case 3:
      return gender == 0 ? 53 : 54; // Dwarf
    case 4:
      return gender == 0 ? 55 : 56; // Night Elf
    case 5:
      return gender == 0 ? 57 : 58; // Undead
    case 6:
      return gender == 0 ? 59 : 60; // Tauren
    case 7:
      return gender == 0 ? 1563 : 1564; // Gnome
    case 8:
      return gender == 0 ? 1478 : 1479; // Troll
    case 9:
      return gender == 0 ? 21245 : 21246; // Goblin
    case 10:
      return gender == 0 ? 15476 : 15475; // Blood Elf
    case 11:
      return gender == 0 ? 16125 : 16126; // Draenei
    case 22:
      return gender == 0 ? 32836 : 32837; // Worgen
    default:
      return 49;
    }
  }

  static std::array<uint32_t, 5> GetDefaultPrimaryStats(uint8 klass) {
    switch (klass) {
    case 1:
      return {23u, 20u, 22u, 20u, 21u}; // Warrior
    case 2:
      return {23u, 20u, 22u, 20u, 22u}; // Paladin
    case 3:
      return {22u, 21u, 22u, 20u, 21u}; // Hunter
    case 4:
      return {23u, 21u, 21u, 20u, 21u}; // Rogue
    case 5:
      return {17u, 22u, 22u, 22u, 23u}; // Priest
    case 6:
      return {25u, 19u, 22u, 20u, 22u}; // Death Knight
    case 7:
      return {22u, 21u, 22u, 20u, 22u}; // Shaman
    case 8:
      return {17u, 22u, 22u, 23u, 23u}; // Mage
    case 9:
      return {21u, 21u, 22u, 23u, 23u}; // Warlock
    case 11:
      return {22u, 20u, 22u, 22u, 23u}; // Druid
    default:
      return {20u, 20u, 20u, 20u, 20u};
    }
  }

  uint32 GetGuid() const { return m_guid; }
  uint32 GetAccount() const { return m_account; }
  const std::string &GetName() const { return m_name; }
  uint8 GetRace() const { return m_race; }
  uint8 GetClass() const { return m_klass; }
  uint8 GetGender() const { return m_gender; }
  uint8 GetSkin() const { return m_skin; }
  uint8 GetFace() const { return m_face; }
  uint8 GetHairStyle() const { return m_hairStyle; }
  uint8 GetHairColor() const { return m_hairColor; }
  uint8 GetFacialHair() const { return m_facialHair; }
  uint8 GetLevel() const { return m_level; }
  uint16 GetZoneId() const { return m_zoneId; }
  uint16 GetMapId() const { return m_mapId; }
  float GetX() const { return m_x; }
  float GetY() const { return m_y; }
  float GetZ() const { return m_z; }
  float GetOrientation() const { return m_o; }
  uint32 GetGuildId() const { return m_guildId; }
  uint32 GetCharacterFlags() const { return m_characterFlags; }
  uint32 GetCustomizationFlags() const { return m_customizationFlags; }
  bool IsFirstLogin() const { return m_firstLogin; }

  std::array<uint32_t, kTutorialMaskInts> GetTutorialMask() const {
    return m_tutorialMask;
  }
  uint8 GetOutfitId() const { return m_outfitId; }
  std::string const &GetEquipmentCache() const { return m_equipmentCache; }

  /// Template entry (`item_instance.itemEntry`) equipped in bag 0 slots 0..18.
  uint32 GetVisibleItemEntry(size_t equipSlot) const {
    if (equipSlot >= kEquipmentSlotCount)
      return 0;
    return m_visibleItems[equipSlot];
  }

  /// Low GUID (`character_inventory.item`) for each equipped slot (bag 0, 0..18).
  uint32 GetVisibleItemGuidLow(size_t equipSlot) const {
    if (equipSlot >= kEquipmentSlotCount)
      return 0;
    return m_visibleItemGuids[equipSlot];
  }

  uint32 GetVisibleItemStackCount(size_t equipSlot) const {
    if (equipSlot >= kEquipmentSlotCount)
      return 0;
    uint32 c = m_visibleItemStacks[equipSlot];
    return c == 0 ? 1u : c;
  }

  uint32 GetPackItemEntry(size_t packIndex) const {
    if (packIndex >= kPackSlotCount)
      return 0;
    return m_packItemEntries[packIndex];
  }

  uint32 GetPackItemGuidLow(size_t packIndex) const {
    if (packIndex >= kPackSlotCount)
      return 0;
    return m_packItemGuids[packIndex];
  }

  uint32 GetPackItemStackCount(size_t packIndex) const {
    if (packIndex >= kPackSlotCount)
      return 0;
    uint32 c = m_packItemStacks[packIndex];
    return c == 0 ? 1u : c;
  }

  Bag0InventoryData GetBag0Inventory() const {
    Bag0InventoryData data;
    for (size_t i = 0; i < kEquipmentSlotCount; ++i) {
      data.equipEntries[i] = m_visibleItems[i];
      data.equipGuids[i] = m_visibleItemGuids[i];
      data.equipStacks[i] = m_visibleItemStacks[i];
    }
    for (size_t i = 0; i < kPackSlotCount; ++i) {
      data.packEntries[i] = m_packItemEntries[i];
      data.packGuids[i] = m_packItemGuids[i];
      data.packStacks[i] = m_packItemStacks[i];
    }
    return data;
  }

  uint32 GetHealth() const { return m_health; }
  uint32 GetMaxHealth() const { return m_maxHealth; }
  uint32 GetFactionTemplate() const { return m_factionTemplate; }
  uint32 GetDisplayId() const { return m_displayId; }

  /// Copper (WoW money unit); persisted in `characters.money`.
  uint32 GetMoney() const { return m_moneyCopper; }

  /// Experience toward next level; persisted in `characters.xp`.
  uint32_t GetXp() const { return m_xp; }
  void SetXp(uint32_t xp) { m_xp = xp; }

  /// STR, AGI, STA, INT, SPI (`UNIT_FIELD_STAT0`..`STAT4`) after world template apply.
  uint32 GetPrimaryStat(uint8_t index) const {
    return index < 5 ? m_primaryStats[index] : 0u;
  }

  uint8 GetPowerType() const { return m_powerType; }
  uint32 GetPower1() const { return m_power1; }
  uint32 GetMaxPower1() const { return m_maxPower1; }

  /// Called after loading from DB using `player_*stats` + optional `gtOCT*.dbc` data.
  void ApplyCombatStateFromTemplate(std::array<uint32_t, 5> const &primaryStats,
                                    uint32 maxHealth, uint32 health,
                                    uint32 power1, uint32 maxPower1,
                                    uint8 powerType) {
    m_primaryStats = primaryStats;
    m_maxHealth = maxHealth;
    m_health = health > maxHealth ? maxHealth : health;
    m_maxPower1 = maxPower1;
    m_power1 = power1 > maxPower1 ? maxPower1 : power1;
    m_powerType = powerType;
  }

  /// Set from `characters.live_*` when loading; consumed by
  /// `ApplyPersistedResourceSnapshotAfterCombatTemplate` after template maxima exist.
  void SetPersistedResourceSnapshotForLogin(std::optional<uint32> health,
                                            std::optional<uint32> power1) {
    m_loginPersistedHealth = health;
    m_loginPersistedPower1 = power1;
  }

  void ApplyPersistedResourceSnapshotAfterCombatTemplate() {
    if (m_loginPersistedHealth.has_value()) {
      m_health = std::min(*m_loginPersistedHealth, m_maxHealth);
      m_loginPersistedHealth.reset();
    }
    if (m_loginPersistedPower1.has_value()) {
      m_power1 = std::min(*m_loginPersistedPower1, m_maxPower1);
      m_loginPersistedPower1.reset();
    }
  }

private:
  uint32 m_guid;
  uint32 m_account;
  std::string m_name;
  uint8 m_race;
  uint8 m_klass;
  uint8 m_gender;
  uint8 m_skin;
  uint8 m_face;
  uint8 m_hairStyle;
  uint8 m_hairColor;
  uint8 m_facialHair;
  uint8 m_level;
  uint16 m_zoneId;
  uint16 m_mapId;
  float m_x;
  float m_y;
  float m_z;
  float m_o;
  uint32 m_guildId;
  uint32 m_characterFlags;
  uint32 m_customizationFlags;
  bool m_firstLogin;
  uint8 m_outfitId;
  std::string m_equipmentCache;
  std::array<uint32_t, kEquipmentSlotCount> m_visibleItems{};
  std::array<uint32_t, kEquipmentSlotCount> m_visibleItemGuids{};
  std::array<uint32_t, kEquipmentSlotCount> m_visibleItemStacks{};
  std::array<uint32_t, kPackSlotCount> m_packItemEntries{};
  std::array<uint32_t, kPackSlotCount> m_packItemGuids{};
  std::array<uint32_t, kPackSlotCount> m_packItemStacks{};

  uint32 m_moneyCopper = 0;
  uint32_t m_xp = 0;
  std::array<uint32_t, kTutorialMaskInts> m_tutorialMask{};

  uint32 m_health;
  uint32 m_maxHealth;
  uint32 m_factionTemplate;
  uint32 m_displayId;

  std::array<uint32_t, 5> m_primaryStats{};
  uint8 m_powerType = 0;
  uint32 m_power1 = 0;
  uint32 m_maxPower1 = 0;

  std::optional<uint32> m_loginPersistedHealth;
  std::optional<uint32> m_loginPersistedPower1;
};

} // namespace Firelands
