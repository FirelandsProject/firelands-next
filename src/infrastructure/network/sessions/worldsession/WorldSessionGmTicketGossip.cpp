#include <application/services/CharacterService.h>
#include <application/services/GmTicketService.h>
#include <application/services/OnlineCharacterSessionRegistry.h>
#include <domain/models/GmTicket.h>
#include <domain/models/NpcText.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/GmTicketGossipUi.h>
#include <infrastructure/network/sessions/worldsession/GossipPackets.h>
#include <infrastructure/network/sessions/worldsession/NpcTextPackets.h>
#include <shared/Logger.h>
#include <algorithm>
#include <optional>
#include <sstream>

namespace Firelands {

namespace {

using namespace gm_ticket_ui;


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
  _gmTicketUi = GmTicketUiSession{};
  _gmTicketUi->gossipNpcGuid = _playerGuid;
  SendGmTicketMainMenu();
}

bool WorldSession::TryBuildGmTicketNpcText(uint32_t textId, NpcText &out) const {
  if (!IsReservedText(textId) || !_gmTicketUi || !_gmTicketService)
    return false;

  out = NpcText::MakeFallback(textId, "");
  out.options[0].probability = 1.f;

  if (textId == kNpcTextMain) {
    out.options[0].text0 =
        "GM ticket desk. Choose a queue or your assigned tickets.";
    out.options[0].text1 = out.options[0].text0;
    return true;
  }

  if (textId == kNpcTextList) {
    std::ostringstream body;
    if (_gmTicketUi->pageTicketIds.empty()) {
      body << "No tickets on this page.";
    } else {
      body << "Select a ticket below.\n";
      for (uint64_t id : _gmTicketUi->pageTicketIds) {
        if (auto t = _gmTicketService->GetById(id)) {
          body << "#" << t->id << " — "
               << CharacterDisplayName(*_charService, t->characterGuid) << " ("
               << StatusLabel(t->status) << ")\n";
        }
      }
    }
    out.options[0].text0 = body.str();
    out.options[0].text1 = out.options[0].text0;
    return true;
  }

  if (textId == kNpcTextDetail) {
    auto const t = _gmTicketService->GetById(_gmTicketUi->selectedTicketId);
    if (!t) {
      out.options[0].text0 = "Ticket no longer available.";
      out.options[0].text1 = out.options[0].text0;
      return true;
    }
    std::ostringstream body;
    body << "Ticket #" << t->id << "\n";
    body << "Character: "
         << CharacterDisplayName(*_charService, t->characterGuid) << "\n";
    body << "Status: " << StatusLabel(t->status) << "\n";
    body << "Map: " << t->mapId << " (" << t->posX << ", " << t->posY << ", "
         << t->posZ << ")\n\n";
    body << "Message:\n"
         << TruncateForGossipOption(t->message, 512) << "\n";
    if (!t->gmResponse.empty())
      body << "\nYour reply:\n" << TruncateForGossipOption(t->gmResponse, 256);
    out.options[0].text0 = TruncateForGossipOption(body.str(), 900);
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

  SendGossipMessage(_gmTicketUi->gossipNpcGuid, kMenuDetail, kNpcTextDetail, items);
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
      _gmTicketUi = std::nullopt;
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
