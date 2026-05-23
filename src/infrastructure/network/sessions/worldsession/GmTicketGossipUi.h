#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Firelands::gm_ticket_ui {

/// Synthetic gossip / npc_text ids (not loaded from `gossip_menu` / `npc_text` tables).
inline constexpr uint32_t kNpcTextMain = 0x7FFEFFE0u;
inline constexpr uint32_t kNpcTextList = 0x7FFEFFE1u;
inline constexpr uint32_t kNpcTextDetail = 0x7FFEFFE2u;

/// Per-ticket detail text ids so the client does not reuse cached `npc_text` for
/// `kNpcTextDetail` when opening different tickets (CMSG_NPC_TEXT_QUERY keys on id).
inline constexpr uint32_t kNpcTextDetailTicketBase = 0x7FFE8000u;
inline constexpr uint32_t kNpcTextDetailTicketLimit =
    kNpcTextMain - kNpcTextDetailTicketBase;

inline constexpr uint32_t kMenuMain = 0x7FFEFF01u;
inline constexpr uint32_t kMenuQueue = 0x7FFEFF02u;
inline constexpr uint32_t kMenuMine = 0x7FFEFF03u;
inline constexpr uint32_t kMenuDetail = 0x7FFEFF04u;

inline constexpr uint32_t kMaxTicketsPerPage = 10u;

/// List menu: ticket rows use `0 .. kMaxTicketsPerPage-1`; navigation uses high indices.
inline constexpr uint32_t kListOptBack = 100u;
inline constexpr uint32_t kListOptNextPage = 101u;
inline constexpr uint32_t kListOptPrevPage = 102u;

enum MainOption : uint32_t {
  MainQueue = 0,
  MainMine = 1,
  MainClose = 2,
};

enum DetailOption : uint32_t {
  DetailTake = 0,
  DetailReply = 1,
  DetailResolve = 2,
  DetailBack = 3,
};

inline bool IsReservedMenu(uint32_t menuId) noexcept {
  return menuId >= kMenuMain && menuId <= kMenuDetail;
}

inline bool IsReservedText(uint32_t textId) noexcept {
  if (textId >= kNpcTextMain && textId <= kNpcTextDetail)
    return true;
  return textId >= kNpcTextDetailTicketBase &&
         textId < kNpcTextDetailTicketBase + kNpcTextDetailTicketLimit;
}

inline uint32_t DetailNpcTextIdForTicket(uint64_t ticketId) noexcept {
  if (ticketId >= kNpcTextDetailTicketLimit)
    return kNpcTextDetail;
  return kNpcTextDetailTicketBase + static_cast<uint32_t>(ticketId);
}

inline std::optional<uint64_t> TicketIdFromDetailNpcTextId(uint32_t textId) noexcept {
  if (textId < kNpcTextDetailTicketBase)
    return std::nullopt;
  uint32_t const offset = textId - kNpcTextDetailTicketBase;
  if (offset >= kNpcTextDetailTicketLimit)
    return std::nullopt;
  return offset;
}

/// Gossip option labels are short on the client; keep a safe cap.
inline std::string TruncateForGossipOption(std::string const &text,
                                           std::size_t maxLen = 96) {
  if (text.size() <= maxLen)
    return text;
  if (maxLen <= 3)
    return text.substr(0, maxLen);
  return text.substr(0, maxLen - 3) + "...";
}

/// Per-session state for the GM ticket gossip desk (kept out of `WorldSession.h`).
struct GmTicketUiSession {
  uint64_t gossipNpcGuid = 0;
  uint64_t selectedTicketId = 0;
  uint32_t listPage = 0;
  enum class ListMode { Queue, Mine } listMode = ListMode::Queue;
  std::vector<uint64_t> pageTicketIds;
};

} // namespace Firelands::gm_ticket_ui
