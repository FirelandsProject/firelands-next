#pragma once

#include <shared/Common.h>
#include <shared/game/InventorySlots.h>
#include <array>
#include <string>
#include <utility>

namespace Firelands {

class Character {
public:
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
            std::array<uint32_t, kPackSlotCount> packItemStacks = {})
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
        m_health(100), m_maxHealth(100),
        m_factionTemplate(1), m_displayId(GetDefaultDisplayId(race, gender)) {}

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

  uint32 GetHealth() const { return m_health; }
  uint32 GetMaxHealth() const { return m_maxHealth; }
  uint32 GetFactionTemplate() const { return m_factionTemplate; }
  uint32 GetDisplayId() const { return m_displayId; }

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

  uint32 m_health;
  uint32 m_maxHealth;
  uint32 m_factionTemplate;
  uint32 m_displayId;
};

} // namespace Firelands
