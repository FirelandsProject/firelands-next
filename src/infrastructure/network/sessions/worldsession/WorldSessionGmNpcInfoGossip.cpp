#include <application/services/WorldService.h>
#include <domain/models/NpcText.h>
#include <domain/repositories/INpcTemplateSearchRepository.h>
#include <domain/world/Aura.h>
#include <domain/world/Creature.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/GmNpcInfoGossipUi.h>
#include <infrastructure/network/sessions/worldsession/GmTicketGossipUi.h>
#include <shared/game/GmGossipDesignTokens.h>
#include <shared/game/PhaseShift.h>
#include <shared/game/UnitCombatStats.h>
#include <shared/network/packets/server/GossipPackets.h>
#include <sstream>
#include <iomanip>

namespace Firelands {

namespace {

using namespace gm_npc_info_ui;
using namespace gm_gossip;
using namespace gm_gossip::color;

GossipMenuItem MakeOption(uint32_t index, std::string text,
                          GossipOptionIcon icon = GossipOptionIcon::Chat,
                          bool coded = false, std::string boxMessage = {}) {
  GossipMenuItem item;
  item.optionIndex = index;
  item.icon = icon;
  item.optionText = gm_ticket_ui::TruncateForGossipOption(std::move(text));
  item.isCoded = coded;
  item.boxMessage = std::move(boxMessage);
  return item;
}

std::string HexU32(uint32 value) {
  std::ostringstream os;
  os << std::hex << std::uppercase << value;
  return os.str();
}

std::string HexU64(uint64 value) {
  std::ostringstream os;
  os << std::hex << std::uppercase << value;
  return os.str();
}

std::string FormatPhaseShift(PhaseShift const &phase) {
  if (phase.phases.empty())
    return "default/unphased";
  std::ostringstream os;
  for (std::size_t i = 0; i < phase.phases.size(); ++i) {
    if (i)
      os << ", ";
    os << phase.phases[i].id;
  }
  os << " (flags=0x" << HexU32(phase.flags) << ")";
  if (phase.personalGuid != 0)
    os << " personal=" << phase.personalGuid;
  return os.str();
}

std::shared_ptr<Creature> TryGetInspectedCreature(WorldSession const &session,
                                                  uint64_t creatureGuid) {
  auto map = session.runtime().GetMap(session.GetMapId());
  if (!map || creatureGuid == 0)
    return {};
  return map->TryGetCreature(creatureGuid);
}

} // namespace

bool WorldSession::OpenGmNpcInfoGossip() {
  if (_playerGuid == 0)
    return false;
  if (_clientSelectionGuid == 0) {
    SendNotification("Select an NPC, then use .npc info.");
    return false;
  }

  auto map = runtime().GetMap(_mapId);
  if (!map)
    return false;

  if (map->TryGetPlayer(_clientSelectionGuid)) {
    SendNotification("Target is a player. Select an NPC for .npc info.");
    return false;
  }

  if (!map->TryGetCreature(_clientSelectionGuid)) {
    SendNotification("Selection is not a creature on this map.");
    return false;
  }

  _gmNpcInfoUi = std::make_unique<GmNpcInfoUiSession>();
  _gmNpcInfoUi->gossipNpcGuid = _playerGuid;
  _gmNpcInfoUi->creatureGuid = _clientSelectionGuid;
  _gmNpcInfoUi->textIds = AllocateNpcInfoTextIds();
  SendGmNpcInfoMainMenu();
  return true;
}

bool WorldSession::GmNpcPrintTargetInfo() { return OpenGmNpcInfoGossip(); }

bool WorldSession::TryBuildGmNpcInfoNpcText(uint32_t textId, NpcText &out) const {
  if (!_gmNpcInfoUi)
    return false;
  auto const page = PageFromTextId(_gmNpcInfoUi->textIds, textId);
  if (!page)
    return false;

  auto cr = TryGetInspectedCreature(*this, _gmNpcInfoUi->creatureGuid);
  if (!cr) {
    out = NpcText::MakeFallback(textId, "Creature is no longer on this map.");
    out.options[0].probability = 1.f;
    return true;
  }

  std::optional<NpcTemplate> tpl;
  if (_npcTemplateSearch)
    tpl = _npcTemplateSearch->TryGetByEntry(cr->GetEntry());

  out = NpcText::MakeFallback(textId, "");
  out.options[0].probability = 1.f;
  std::ostringstream body;

  switch (*page) {
  case NpcInfoTextPage::Main:
    AppendPageTitle(body, "GM NPC Inspector");
    AppendRule(body);
    AppendLabelValue(body, "Guid", std::to_string(cr->GetGuid()));
    AppendLabelValue(body, "Entry", std::to_string(cr->GetEntry()), kAccent);
    if (tpl) {
      body << kLabel << "Name:|r " << kValue << tpl->name << kReset;
      if (!tpl->subname.empty())
        body << "  " << kSubname << tpl->subname << kReset;
      body << kLine;
    }
    AppendLabelValue(body, "Display", std::to_string(cr->GetDisplayId()));
    AppendLabelValue(body, "Level", std::to_string(static_cast<unsigned>(cr->GetLevel())));
    AppendLabelValue(body, "Health",
                     std::to_string(cr->GetLiveHealth()) + " / " +
                         std::to_string(cr->GetLiveMaxHealth()));
    AppendLabelValue(body, "Faction", std::to_string(cr->GetFactionTemplate()));
    AppendLabelValue(body, "UNIT_NPC_FLAGS", "0x" + HexU32(cr->GetNpcFlags()));
    AppendLabelValue(body, "UNIT_FIELD_FLAGS", "0x" + HexU32(cr->GetUnitFieldFlags()));
    if (cr->GetUnitFieldFlags2() != 0)
      AppendLabelValue(body, "UNIT_FIELD_FLAGS_2", "0x" + HexU32(cr->GetUnitFieldFlags2()));
    if (cr->GetExtraFlags() != 0)
      AppendLabelValue(body, "Template extra flags", "0x" + HexU32(cr->GetExtraFlags()));
    if (cr->ActsAsScriptTrigger()) {
      body << kLine << kMuted
           << "Script-trigger unit: no UNIT_NPC_FLAGS on template; uses unit field + "
              "extra flags (typical quest proxy)."
           << kReset;
    }
    body << kParagraph << kMuted
         << "Use the options below for template DB, live state, and combat details."
         << kReset;
    break;
  case NpcInfoTextPage::Template:
    AppendPageTitle(body, "creature_template");
    body << kChevron << " > " << kReset << kAccent << "entry " << cr->GetEntry()
         << kReset << kLine;
    AppendRule(body);
    if (!tpl) {
      body << kWarning << "No row in creature_template for this entry." << kReset << kLine;
      body << kMuted << "(Apply migrations / import template data.)" << kReset;
    } else {
      AppendLabelValue(body, "Name", tpl->name);
      if (!tpl->subname.empty())
        AppendLabelValue(body, "Subname", tpl->subname, kSubname);
      AppendLabelValue(body, "Faction template", std::to_string(tpl->factionTemplate));
      AppendLabelValue(body, "Gossip menu", std::to_string(tpl->gossipMenuId));
      AppendLabelValue(body, "npcflag (DB)", "0x" + HexU64(tpl->npcFlags));
      AppendLabelValue(body, "unit_flags (DB)", "0x" + HexU32(tpl->unitFieldFlags));
      if (tpl->unitFieldFlags2 != 0)
        AppendLabelValue(body, "unit_flags2 (DB)", "0x" + HexU32(tpl->unitFieldFlags2));
      if (tpl->extraFlags != 0)
        AppendLabelValue(body, "flags_extra (DB)", "0x" + HexU32(tpl->extraFlags));
      AppendSectionHeader(body, "Display models");
      for (std::size_t i = 0; i < tpl->displayIds.size(); ++i) {
        if (tpl->displayIds[i] != 0)
          AppendLabelValue(body, ("Slot " + std::to_string(i)).c_str(),
                           std::to_string(tpl->displayIds[i]));
      }
      AppendSectionHeader(body, "Combat spells (template)");
      if (tpl->combatSpells.empty()) {
        body << kMuted << "(none)" << kReset;
      } else {
        for (uint32_t sid : tpl->combatSpells)
          body << kAccent << sid << kReset << kLine;
      }
    }
    break;
  case NpcInfoTextPage::Live: {
    MovementInfo const &pos = cr->GetPosition();
    AppendPageTitle(body, "Live world state");
    AppendRule(body);
    AppendLabelValue(body, "Map", std::to_string(_mapId));
    AppendLabelValue(body, "Position",
                     "(" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " +
                         std::to_string(pos.z) + ")");
    AppendLabelValue(body, "Orientation", std::to_string(pos.orientation));
    AppendLabelValue(body, "Phase", FormatPhaseShift(cr->GetPhaseShift()));
    AppendLabelValue(body, "XP modifier", std::to_string(cr->GetExperienceModifier()));
    if (cr->IsEvading())
      AppendLabelValue(body, "State", "evading", kWarning);
    if (uint64 const chase = cr->GetChaseTargetPlayerGuid(); chase != 0)
      AppendLabelValue(body, "Chase target", std::to_string(chase));
    AppendSectionHeader(body, "Spawn helpers");
    body << kMuted << ".npc add " << kReset << kAccent << cr->GetEntry() << kReset << " "
         << kValue << cr->GetDisplayId() << " " << cr->GetFactionTemplate() << kReset
         << kLine;
    body << kMuted << ".npc del" << kReset << "  " << kChevron << "(target this unit)"
         << kReset;
    break;
  }
  case NpcInfoTextPage::Combat: {
    UnitCombatStats const &stats = cr->GetCombatStats();
    AppendPageTitle(body, "Combat & auras");
    AppendRule(body);
    AppendLabelValue(body, "Level (stats)", std::to_string(static_cast<unsigned>(stats.level)));
    AppendLabelValue(body, "Armor", std::to_string(stats.armor));
    AppendLabelValue(body, "Attack power", std::to_string(EffectiveAttackPower(stats)));
    AppendSectionHeader(body, "Resistances");
    static char const *kSchools[] = {"Physical", "Holy",   "Fire",  "Nature",
                                     "Frost",    "Shadow", "Arcane"};
    for (std::size_t i = 0; i < stats.resistance.size() && i < 7; ++i) {
      AppendLabelValue(body, kSchools[i],
                       std::to_string(EffectiveSchoolResistance(stats,
                                                                static_cast<uint8>(i))));
    }
    AppendSectionHeader(body, "Active auras");
    auto const auras = cr->GetActiveAuras();
    if (auras.empty()) {
      body << kMuted << "(none)" << kReset;
    } else {
      std::size_t shown = 0;
      for (Aura const &aura : auras) {
        if (shown >= 12) {
          body << kMuted << "(more auras not shown)" << kReset;
          break;
        }
        body << kAccent << aura.GetSpellId() << kReset;
        if (aura.GetCasterGuid() != 0)
          body << "  " << kMuted << "caster " << aura.GetCasterGuid() << kReset;
        body << kLine;
        ++shown;
      }
    }
    break;
  }
  }

  std::string const text = TruncateBody(body.str());
  out.options[0].text0 = text;
  out.options[0].text1 = text;
  return true;
}

void WorldSession::SendGmNpcInfoMainMenu() {
  if (!_gmNpcInfoUi)
    return;
  std::vector<GossipMenuItem> items;
  items.push_back(MakeOption(MainTemplate, "Template (creature_template)",
                             GossipOptionIcon::Dot));
  items.push_back(MakeOption(MainLive, "Live world state", GossipOptionIcon::Dot));
  items.push_back(MakeOption(MainCombat, "Combat & auras", GossipOptionIcon::Dot));
  items.push_back(MakeOption(MainClose, "Close", GossipOptionIcon::Chat));
  SendGossipMessage(_gmNpcInfoUi->gossipNpcGuid, kMenuMain, _gmNpcInfoUi->textIds.main,
                    items);
}

void WorldSession::SendGmNpcInfoSubMenu(uint32_t menuId, uint32_t textId) {
  if (!_gmNpcInfoUi)
    return;
  std::vector<GossipMenuItem> items;
  items.push_back(MakeOption(kOptBack, "Back", GossipOptionIcon::Chat11));
  SendGossipMessage(_gmNpcInfoUi->gossipNpcGuid, menuId, textId, items);
}

bool WorldSession::TryHandleGmNpcInfoGossipSelect(uint64_t npcGuid, uint32_t menuId,
                                                  uint32_t listId,
                                                  std::string const &code) {
  (void)code;
  if (!IsReservedMenu(menuId) || !_gmNpcInfoUi)
    return false;
  if (npcGuid != _gmNpcInfoUi->gossipNpcGuid)
    return false;

  if (menuId == kMenuMain) {
    switch (listId) {
    case MainTemplate:
      SendGmNpcInfoSubMenu(kMenuTemplate,
                           TextIdForPage(_gmNpcInfoUi->textIds, NpcInfoTextPage::Template));
      return true;
    case MainLive:
      SendGmNpcInfoSubMenu(kMenuLive,
                           TextIdForPage(_gmNpcInfoUi->textIds, NpcInfoTextPage::Live));
      return true;
    case MainCombat:
      SendGmNpcInfoSubMenu(kMenuCombat,
                           TextIdForPage(_gmNpcInfoUi->textIds, NpcInfoTextPage::Combat));
      return true;
    case MainClose:
      _gmNpcInfoUi.reset();
      SendGossipComplete();
      return true;
    default:
      SendGossipComplete();
      return true;
    }
  }

  if (menuId == kMenuTemplate || menuId == kMenuLive || menuId == kMenuCombat) {
    if (listId == kOptBack) {
      SendGmNpcInfoMainMenu();
      return true;
    }
    SendGmNpcInfoMainMenu();
    return true;
  }

  return false;
}

} // namespace Firelands
