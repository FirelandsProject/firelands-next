#include "Realm.h"

namespace Firelands {

Realm::Realm(uint32_t id, std::string name, std::string address, uint16_t port,
             uint8_t icon, uint8_t timezone, uint8_t allowedSecurityLevel,
             float population, uint8_t realmListFlags)
    : m_id(id), m_name(std::move(name)), m_address(std::move(address)),
      m_icon(icon), m_timezone(timezone),
      m_allowedSecurityLevel(allowedSecurityLevel),
      m_realmListFlags(realmListFlags) {
  if (port == 0) {
    throw std::invalid_argument("Port cannot be 0");
  }
  m_port = port;
  m_population = (population < 0.0f) ? 0.0f : population;
}

uint32_t Realm::GetId() const { return m_id; }
const std::string &Realm::GetName() const { return m_name; }
const std::string &Realm::GetAddress() const { return m_address; }
uint16_t Realm::GetPort() const { return m_port; }
uint8_t Realm::GetIcon() const { return m_icon; }
uint8_t Realm::GetTimezone() const { return m_timezone; }
uint8_t Realm::GetAllowedSecurityLevel() const {
  return m_allowedSecurityLevel;
}
float Realm::GetPopulation() const { return m_population; }

uint8_t Realm::GetRealmListFlags() const { return m_realmListFlags; }

} // namespace Firelands
