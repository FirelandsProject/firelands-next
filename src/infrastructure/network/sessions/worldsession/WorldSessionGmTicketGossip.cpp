#include <application/services/CharacterService.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <domain/models/GmTicket.h>
#include <domain/models/NpcText.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/GmTicketGossipUi.h>
#include <shared/game/GmGossipDesignTokens.h>
#include <shared/network/packets/server/GossipPackets.h>
#include <shared/network/packets/server/NpcTextPackets.h>
#include <shared/Logger.h>
#include <algorithm>
#include <optional>
#include <sstream>

namespace Firelands {

namespace {

using namespace gm_ticket_ui;
using namespace gm_gossip;
using namespace gm_gossip::color;

std::string StatusLabel(GmTicketStatus status) {
  switch (status) {
  case GmTicketStatus::Open:
    return "Open";
  case GmTicketStatus::Assigned:
    return "Assigned";
  case GmTicketStatus::GmAnswered:
    return "Answered";
  default:
    return "Active";
  }
}

std::string CharacterDisplayName(CharacterService &characters, uint32_t characterGuid) {
  if (auto ch = characters.GetCharacterByGuid(characterGuid))
    return ch->GetName();
  return "char#" + std::to_string(characterGuid);
}

GossipMenuItem MakeOption(uint32_t index, std::string text,
                          GossipOptionIcon icon = GossipOptionIcon::Chat,
                          bool coded = false, std::string boxMessage = {}) {
  GossipMenuItem item;
  item.optionIndex = index;
  item.icon = icon;
  item.optionText = std::move(text);
  item.isCoded = coded;
  item.boxMessage = std::move(boxMessage);
  return item;
}

} // namespace

void WorldSession::OpenGmTicketUi() {
  if (_playerGuid == 0) {
    SendNotification("You must be in the world to use the ticket UI.");
    return;
  }
  if (!_gmTicketService) {
    SendNotification("Ticket system is not configured.");
    return;
  }
  _gmTicketUi = std::make_unique<GmTicketUiSession>();
  _gmTicketUi->gossipNpcGuid = _playerGuid;
  SendGmTicketMainMenu();
}

bool WorldSession::TryBuildGmTicketNpcText(uint32_t textId, NpcText &out) const {
  if (!IsReservedText(textId) || !_gmTicketUi || !_gmTicketService)
    return false;

  out = NpcText::MakeFallback(textId, "");
  out.options[0].probability = 1.f;

  if (textId == kNpcTextMain) {
    std::ostringstream body;
    AppendPageTitle(body, "GM ticket desk");
    AppendRule(body);
    body << kMuted << "Choose a queue or your assigned tickets below." << kReset;
    out.options[0].text0 = body.str();
    out.options[0].text1 = out.options[0].text0;
    return true;
  }

  if (textId == kNpcTextList) {
    std::ostringstream body;
    AppendPageTitle(body, "Ticket list");
    AppendRule(body);
    if (_gmTicketUi->pageTicketIds.empty()) {
      body << kMuted << "No tickets on this page." << kReset;
    } else {
      body << kMuted << "Select a ticket below." << kReset << kLine;
      for (uint64_t id : _gmTicketUi->pageTicketIds) {
        if (auto t = _gmTicketService->GetById(id)) {
          body << kAccent << "#" << t->id << kReset << " " << kChevron << " " << kReset
               << kValue << CharacterDisplayName(*_charService, t->characterGuid) << kReset
               << " " << kSubname << "(" << StatusLabel(t->status) << ")" << kReset
               << kLine;
        }
      }
    }
    out.options[0].text0 = TruncateBody(body.str());
    out.options[0].text1 = out.options[0].text0;
    return true;
  }

  if (textId == kNpcTextDetail || TicketIdFromDetailNpcTextId(textId)) {
    uint64_t ticketId = _gmTicketUi->selectedTicketId;
    if (auto const fromText = TicketIdFromDetailNpcTextId(textId))
      ticketId = *fromText;
    auto const t = _gmTicketService->GetById(ticketId);
    if (!t) {
      std::ostringstream body;
      body << kWarning << "Ticket no longer available." << kReset;
      out.options[0].text0 = body.str();
      out.options[0].text1 = out.options[0].text0;
      return true;
    }
    std::ostringstream body;
    AppendPageTitle(body, "Ticket #" + std::to_string(t->id));
    AppendRule(body);
    AppendLabelValue(body, "Character",
                     CharacterDisplayName(*_charService, t->characterGuid));
    AppendLabelValue(body, "Status", StatusLabel(t->status));
    AppendLabelValue(body, "Map",
                     std::to_string(t->mapId) + " (" + std::to_string(t->posX) + ", " +
                         std::to_string(t->posY) + ", " + std::to_string(t->posZ) + ")");
    AppendSectionHeader(body, "Player message");
    body << kValue << TruncateForGossipOption(t->message, 512) << kReset;
    if (!t->gmResponse.empty()) {
      AppendSectionHeader(body, "Your reply");
      body << kValue << TruncateForGossipOption(t->gmResponse, 256) << kReset;
    }
    out.options[0].text0 = TruncateBody(body.str());
    out.options[0].text1 = out.options[0].text0;
    return true;
  }

  return false;
}

void WorldSession::SendGmTicketMainMenu() {
  if (!_gmTicketUi)
    return;
  std::vector<GossipMenuItem> items;
  items.push_back(MakeOption(MainQueue, "Open ticket queue", GossipOptionIcon::Dot));
  items.push_back(MakeOption(MainMine, "My assigned tickets", GossipOptionIcon::Dot));
  items.push_back(MakeOption(MainClose, "Close", GossipOptionIcon::Chat));
  SendGossipMessage(_gmTicketUi->gossipNpcGuid, kMenuMain, kNpcTextMain, items);
}

void WorldSession::SendGmTicketListMenu() {
  if (!_gmTicketUi || !_gmTicketService)
    return;

  std::vector<GmTicket> source;
  if (_gmTicketUi->listMode == GmTicketUiSession::ListMode::Queue)
    source = _gmTicketService->ListQueue(200);
  else
    source = _gmTicketService->ListAssignedTo(_accountId, 200);

  uint32_t const page = _gmTicketUi->listPage;
  uint32_t const start = page * kMaxTicketsPerPage;
  uint32_t const end =
      std::min<uint32_t>(start + kMaxTicketsPerPage, static_cast<uint32_t>(source.size()));

  _gmTicketUi->pageTicketIds.clear();
  std::vector<GossipMenuItem> items;
  for (uint32_t i = start; i < end; ++i) {
    auto const &t = source[i];
    _gmTicketUi->pageTicketIds.push_back(t.id);
    std::string label = "#" + std::to_string(t.id) + " " +
                        CharacterDisplayName(*_charService, t.characterGuid);
    if (!t.message.empty()) {
      label += " — ";
      label += TruncateForGossipOption(t.message, 48);
    }
    items.push_back(MakeOption(i - start, std::move(label), GossipOptionIcon::Chat));
  }

  if (page > 0)
    items.push_back(
        MakeOption(kListOptPrevPage, "Previous page", GossipOptionIcon::Chat11));
  if (end < source.size())
    items.push_back(
        MakeOption(kListOptNextPage, "Next page", GossipOptionIcon::Chat11));
  items.push_back(MakeOption(kListOptBack, "Back", GossipOptionIcon::Chat));

  uint32_t const menuId =
      _gmTicketUi->listMode == GmTicketUiSession::ListMode::Queue ? kMenuQueue
                                                                    : kMenuMine;
  SendGossipMessage(_gmTicketUi->gossipNpcGuid, menuId, kNpcTextList, items);
}

void WorldSession::SendGmTicketDetailMenu() {
  if (!_gmTicketUi || !_gmTicketService)
    return;

  auto const t = _gmTicketService->GetById(_gmTicketUi->selectedTicketId);
  if (!t) {
    SendNotification("Ticket not found.");
    SendGmTicketMainMenu();
    return;
  }

  bool const assignedToMe =
      t->assignedAccountId.has_value() &&
      *t->assignedAccountId == _accountId;
  bool const unassigned = !t->assignedAccountId.has_value() &&
                          t->status == GmTicketStatus::Open;

  std::vector<GossipMenuItem> items;
  if (unassigned || !assignedToMe)
    items.push_back(MakeOption(DetailTake, "Take ticket", GossipOptionIcon::Battle));
  if (assignedToMe) {
    items.push_back(MakeOption(DetailReply, "Write reply…", GossipOptionIcon::Chat,
                               true, "Enter your reply to the player:"));
    items.push_back(
        MakeOption(DetailResolve, "Mark resolved (close)", GossipOptionIcon::Dot));
  }
  items.push_back(MakeOption(DetailBack, "Back", GossipOptionIcon::Chat));

  SendGossipMessage(_gmTicketUi->gossipNpcGuid, kMenuDetail,
                    DetailNpcTextIdForTicket(t->id), items);
}

void WorldSession::NotifyPlayerGmTicketReply(GmTicket const &ticket) {
  if (!_charService || !_onlineCharRegistry)
    return;
  auto ch = _charService->GetCharacterByGuid(ticket.characterGuid);
  if (!ch)
    return;
  if (auto target = _onlineCharRegistry->TryResolve(ch->GetName())) {
    target->SendGmResponseReceived(static_cast<uint32_t>(ticket.id), ticket.message,
                                   ticket.gmResponse);
  }
}

bool WorldSession::TryHandleGmTicketGossipSelect(uint64_t npcGuid, uint32_t menuId,
                                                 uint32_t listId,
                                                 std::string const &code) {
  if (!IsReservedMenu(menuId) || !_gmTicketUi || !_gmTicketService)
    return false;
  if (npcGuid != _gmTicketUi->gossipNpcGuid)
    return false;

  if (menuId == kMenuMain) {
    switch (listId) {
    case MainQueue:
      _gmTicketUi->listMode = GmTicketUiSession::ListMode::Queue;
      _gmTicketUi->listPage = 0;
      SendGmTicketListMenu();
      return true;
    case MainMine:
      _gmTicketUi->listMode = GmTicketUiSession::ListMode::Mine;
      _gmTicketUi->listPage = 0;
      SendGmTicketListMenu();
      return true;
    case MainClose:
      _gmTicketUi.reset();
      SendGossipComplete();
      return true;
    default:
      SendGossipComplete();
      return true;
    }
  }

  if (menuId == kMenuQueue || menuId == kMenuMine) {
    if (listId == kListOptBack) {
      SendGmTicketMainMenu();
      return true;
    }
    if (listId == kListOptNextPage) {
      ++_gmTicketUi->listPage;
      SendGmTicketListMenu();
      return true;
    }
    if (listId == kListOptPrevPage && _gmTicketUi->listPage > 0) {
      --_gmTicketUi->listPage;
      SendGmTicketListMenu();
      return true;
    }
    if (listId < _gmTicketUi->pageTicketIds.size()) {
      _gmTicketUi->selectedTicketId = _gmTicketUi->pageTicketIds[listId];
      SendGmTicketDetailMenu();
      return true;
    }
    SendGmTicketListMenu();
    return true;
  }

  if (menuId == kMenuDetail) {
    if (listId == DetailBack) {
      SendGmTicketListMenu();
      return true;
    }

    uint64_t const ticketId = _gmTicketUi->selectedTicketId;
    if (listId == DetailTake) {
      if (_gmTicketService->AssignToStaff(ticketId, _accountId)) {
        SendNotification("Ticket assigned to you.");
        SendGmTicketDetailMenu();
      } else {
        SendNotification("Could not take ticket (already assigned or closed).");
        SendGmTicketDetailMenu();
      }
      return true;
    }

    if (listId == DetailReply) {
      if (code.empty()) {
        SendNotification("Type your reply in the text box and confirm.");
        return true;
      }
      if (!_gmTicketService->StaffReply(ticketId, _accountId, code)) {
        SendNotification("Reply failed (take the ticket first or bad state).");
        SendGmTicketDetailMenu();
        return true;
      }
      if (auto updated = _gmTicketService->GetById(ticketId)) {
        NotifyPlayerGmTicketReply(*updated);
        SendNotification("Reply sent to the player.");
      } else {
        SendNotification("Reply saved.");
      }
      SendGmTicketDetailMenu();
      return true;
    }

    if (listId == DetailResolve) {
      if (_gmTicketService->StaffClose(ticketId, _accountId)) {
        SendNotification("Ticket closed.");
        _gmTicketUi->selectedTicketId = 0;
        SendGmTicketListMenu();
      } else {
        SendNotification("Could not close ticket (take it first or already closed).");
        SendGmTicketDetailMenu();
      }
      return true;
    }

    SendGmTicketDetailMenu();
    return true;
  }

  return false;
}

} // namespace Firelands
