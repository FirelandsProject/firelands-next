#pragma once

#include <cstdint>
#include <string>

namespace Firelands::gm_ticket_ui {

/// Synthetic gossip / npc_text ids (not loaded from `gossip_menu` / `npc_text` tables).
inline constexpr uint32_t kNpcTextMain = 0x7FFEFFE0u;
inline constexpr uint32_t kNpcTextList = 0x7FFEFFE1u;
inline constexpr uint32_t kNpcTextDetail = 0x7FFEFFE2u;

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
  return textId >= kNpcTextMain && textId <= kNpcTextDetail;
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

} // namespace Firelands::gm_ticket_ui
