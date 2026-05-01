#pragma once

#include <cstdint>
#include <string>

namespace Firelands {

class Realm {
public:
  Realm(uint32_t id, std::string name, std::string address, uint16_t port,
        uint8_t icon, uint8_t timezone, uint8_t allowedSecurityLevel,
        float population, uint8_t realmListFlags = 0);

  uint32_t GetId() const;
  const std::string &GetName() const;
  const std::string &GetAddress() const;
  uint16_t GetPort() const;
  uint8_t GetIcon() const;
  uint8_t GetTimezone() const;
  uint8_t GetAllowedSecurityLevel() const;
  float GetPopulation() const;
  /// AUTH_REALM_LIST third byte per realm (e.g. Trinity REALM_FLAG_OFFLINE = 2).
  uint8_t GetRealmListFlags() const;

private:
  uint32_t m_id;
  std::string m_name;
  std::string m_address;
  uint16_t m_port;
  uint8_t m_icon;
  uint8_t m_timezone;
  uint8_t m_allowedSecurityLevel;
  float m_population;
  uint8_t m_realmListFlags;
};

} // namespace Firelands
