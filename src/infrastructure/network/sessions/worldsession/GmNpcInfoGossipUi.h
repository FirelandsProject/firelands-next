#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

namespace Firelands::gm_npc_info_ui {

/// Synthetic gossip menu ids for `.npc info` (not in world DB).
inline constexpr uint32_t kMenuMain = 0x7FFEF100u;
inline constexpr uint32_t kMenuTemplate = 0x7FFEF101u;
inline constexpr uint32_t kMenuLive = 0x7FFEF102u;
inline constexpr uint32_t kMenuCombat = 0x7FFEF103u;

inline constexpr uint32_t kOptBack = 100u;

enum MainOption : uint32_t {
  MainTemplate = 0,
  MainLive = 1,
  MainCombat = 2,
  MainClose = 3,
};

enum class NpcInfoTextPage : uint8_t {
  Main = 0,
  Template = 1,
  Live = 2,
  Combat = 3,
};

inline constexpr uint32_t kNpcTextPagesPerSession = 4u;

/// Client caches `npc_text` by id (`CMSG_NPC_TEXT_QUERY`). Fresh ids per `.npc info` open
/// (same pattern as `gm_ticket_ui::DetailNpcTextIdForTicket`). Must stay below ticket desk
/// detail pool (`0x7FFE8000`).
inline constexpr uint32_t kNpcTextSessionBase = 0x7FFEC000u;
inline constexpr uint32_t kNpcTextSessionSlots = 2048u;
inline constexpr uint32_t kNpcTextSessionSpan =
    kNpcTextSessionSlots * kNpcTextPagesPerSession;

struct NpcInfoTextIds {
  uint32_t main = 0;
  uint32_t templ = 0;
  uint32_t live = 0;
  uint32_t combat = 0;
};

inline NpcInfoTextIds AllocateNpcInfoTextIds() noexcept {
  static std::atomic<uint32_t> nextSlot{0};
  uint32_t const slot =
      nextSlot.fetch_add(1, std::memory_order_relaxed) % kNpcTextSessionSlots;
  uint32_t const base = kNpcTextSessionBase + slot * kNpcTextPagesPerSession;
  return {base + 0, base + 1, base + 2, base + 3};
}

inline bool IsReservedText(uint32_t textId) noexcept {
  return textId >= kNpcTextSessionBase &&
         textId < kNpcTextSessionBase + kNpcTextSessionSpan;
}

inline std::optional<NpcInfoTextPage> PageFromTextId(NpcInfoTextIds const &ids,
                                                     uint32_t textId) noexcept {
  if (textId == ids.main)
    return NpcInfoTextPage::Main;
  if (textId == ids.templ)
    return NpcInfoTextPage::Template;
  if (textId == ids.live)
    return NpcInfoTextPage::Live;
  if (textId == ids.combat)
    return NpcInfoTextPage::Combat;
  return std::nullopt;
}

inline uint32_t TextIdForPage(NpcInfoTextIds const &ids, NpcInfoTextPage page) noexcept {
  switch (page) {
  case NpcInfoTextPage::Main:
    return ids.main;
  case NpcInfoTextPage::Template:
    return ids.templ;
  case NpcInfoTextPage::Live:
    return ids.live;
  case NpcInfoTextPage::Combat:
    return ids.combat;
  }
  return ids.main;
}

inline bool IsReservedMenu(uint32_t menuId) noexcept {
  return menuId >= kMenuMain && menuId <= kMenuCombat;
}

/// Per-session state for `.npc info` gossip (kept out of `WorldSession.h`).
struct GmNpcInfoUiSession {
  /// Cataclysm gossip must target a unit the client already owns (ticket desk uses self).
  uint64_t gossipNpcGuid = 0;
  /// Creature under inspection (map lookup); not sent on the gossip wire.
  uint64_t creatureGuid = 0;
  NpcInfoTextIds textIds{};
};

} // namespace Firelands::gm_npc_info_ui
