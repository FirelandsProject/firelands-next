#pragma once

#include <domain/models/GmTicket.h>
#include <shared/network/WorldPacket.h>
#include <cstdint>
#include <optional>
#include <string>

namespace Firelands {

/// Cataclysm 4.3.4 GM ticket wire helpers (aligned with `GmTicket::WritePacket` /
/// `TicketMgr::SendTicket` and opcode names in `WorldOpcodes.h`).
namespace gm_ticket {

constexpr uint32_t kWireStatusHasText = 0x06;
constexpr uint32_t kWireStatusDefault = 0x0A;
constexpr uint32_t kWireResponseCreateSuccess = 2;
constexpr uint32_t kWireResponseCreateError = 3;
constexpr uint32_t kWireResponseAlreadyExist = 1;
constexpr uint32_t kWireResponseUpdateSuccess = 4;
constexpr uint32_t kWireResponseUpdateError = 5;
constexpr uint32_t kWireResponseTicketDeleted = 9;
constexpr uint32_t kQueueStatusEnabled = 1;
constexpr uint32_t kQueueStatusDisabled = 0;
constexpr uint8_t kEscalationUnassigned = 0;
constexpr uint8_t kEscalationAssigned = 1;
constexpr uint8_t kEscalationCap = 2;
constexpr uint8_t kOpenedByGmNot = 0;
constexpr uint8_t kOpenedByGmYes = 1;

float AgeInDays(uint64_t nowUnixSec, uint64_t eventUnixSec);

/// `SMSG_GM_TICKET_GET_TICKET` body when the player has an active ticket (not yet
/// in "GM answered, show response" mode — that uses `SendGmResponseReceived`).
void BuildGetTicketHasText(WorldPacket &out, GmTicket const &ticket,
                           std::optional<uint64_t> oldestActiveUpdatedUnix,
                           uint64_t lastQueueChangeUnix);

void BuildGetTicketNoTicket(WorldPacket &out);

void BuildGmResponseReceived(WorldPacket &out, uint32_t ticketId,
                             std::string const &playerMessage,
                             std::string const &gmResponse);

struct ParsedTicketCreate {
  uint32_t mapId = 0;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  std::string message;
  uint32_t needResponse = 0;
  bool needMoreHelp = false;
};

enum class ParseCreateResult { Ok, TooShort, BadPayload };

/// Reads and consumes `CMSG_GM_TICKET_CREATE` payload (including optional zlib chat log).
ParseCreateResult TryParseTicketCreate(WorldPacket &packet, ParsedTicketCreate &out);

} // namespace gm_ticket

} // namespace Firelands
