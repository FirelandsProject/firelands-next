#include <application/ports/IMapNotifier.h>
#include <application/services/WorldService.h>
#include <domain/models/Chat.h>
#include <application/ports/ICommandSession.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/game/ChatLanguages.h>
#include <shared/Logger.h>
#include <shared/network/BitReader.h>
#include <shared/network/BitWriter.h>
#include <shared/network/WorldOpcodes.h>

namespace Firelands {

namespace {

bool DecodeStandardChatOpcode(uint32 opcode, uint32 &outType) {
  switch (opcode) {
  case CMSG_MESSAGECHAT_SAY:
    outType = CHAT_MSG_SAY;
    return true;
  case CMSG_MESSAGECHAT_YELL:
    outType = CHAT_MSG_YELL;
    return true;
  case CMSG_MESSAGECHAT_CHANNEL:
    outType = CHAT_MSG_CHANNEL;
    return true;
  case CMSG_MESSAGECHAT_WHISPER:
    outType = CHAT_MSG_WHISPER;
    return true;
  case CMSG_MESSAGECHAT_GUILD:
    outType = CHAT_MSG_GUILD;
    return true;
  case CMSG_MESSAGECHAT_OFFICER:
    outType = CHAT_MSG_OFFICER;
    return true;
  case CMSG_MESSAGECHAT_AFK:
    outType = CHAT_MSG_AFK;
    return true;
  case CMSG_MESSAGECHAT_DND:
    outType = CHAT_MSG_DND;
    return true;
  case CMSG_MESSAGECHAT_EMOTE:
    outType = CHAT_MSG_EMOTE;
    return true;
  case CMSG_MESSAGECHAT_PARTY:
    outType = CHAT_MSG_PARTY;
    return true;
  case CMSG_MESSAGECHAT_RAID:
    outType = CHAT_MSG_RAID;
    return true;
  case CMSG_MESSAGECHAT_BATTLEGROUND:
    outType = CHAT_MSG_BATTLEGROUND;
    return true;
  case CMSG_MESSAGECHAT_RAID_WARNING:
    outType = CHAT_MSG_RAID_WARNING;
    return true;
  default:
    return false;
  }
}

bool DecodeAddonChatOpcode(uint32 opcode, uint32 &outType) {
  switch (opcode) {
  case CMSG_MESSAGECHAT_ADDON_BATTLEGROUND:
    outType = CHAT_MSG_BATTLEGROUND;
    return true;
  case CMSG_MESSAGECHAT_ADDON_GUILD:
    outType = CHAT_MSG_GUILD;
    return true;
  case CMSG_MESSAGECHAT_ADDON_OFFICER:
    outType = CHAT_MSG_OFFICER;
    return true;
  case CMSG_MESSAGECHAT_ADDON_PARTY:
    outType = CHAT_MSG_PARTY;
    return true;
  case CMSG_MESSAGECHAT_ADDON_RAID:
    outType = CHAT_MSG_RAID;
    return true;
  case CMSG_MESSAGECHAT_ADDON_WHISPER:
    outType = CHAT_MSG_WHISPER;
    return true;
  default:
    return false;
  }
}

bool AddonLangAllowedForType(uint32 type) {
  switch (type) {
  case CHAT_MSG_PARTY:
  case CHAT_MSG_RAID:
  case CHAT_MSG_GUILD:
  case CHAT_MSG_WHISPER:
  case CHAT_MSG_BATTLEGROUND:
    return true;
  default:
    return false;
  }
}

/// Cataclysm 4.3.4 layout aligned with `ChatHandler::BuildChatPacket` (default
/// branch): optional GM sender name only for `SMSG_GM_MESSAGECHAT`, then optional
/// channel name, target GUID, message, `ChatTag` byte (not a second null).
void AppendSmsgMessageChatPayload(WorldPacket &data, uint32 chatType, uint32 lang,
                                  uint64 senderGuid, uint64 receiverGuid,
                                  std::string const &message,
                                  std::string const *channelNameOptional,
                                  bool gmMessage,
                                  std::string const &senderNameForGmPacket,
                                  uint8 chatTag) {
  data.Append<uint8>(static_cast<uint8>(chatType));
  int32 const langWire =
      (lang == CHAT_LANG_ADDON) ? -1 : static_cast<int32>(lang);
  data.Append<int32>(langWire);
  data.Append<uint64>(senderGuid);
  data.Append<uint32>(0);
  if (gmMessage) {
    data.Append<uint32>(
        static_cast<uint32>(senderNameForGmPacket.length() + 1));
    data.WriteString(senderNameForGmPacket);
  }
  if (channelNameOptional && chatType == CHAT_MSG_CHANNEL)
    data.WriteString(*channelNameOptional);
  data.Append<uint64>(receiverGuid);
  data.Append<uint32>(static_cast<uint32>(message.length() + 1));
  data.WriteString(message);
  data.Append<uint8>(chatTag);
}

/// Build 15595 uses **per-opcode** chat (`CMSG_MESSAGECHAT_SAY`, etc.); the first
/// uint32 in the payload is **language only** (`Language` from SharedDefines.h).
/// Do not reinterpret that field as `ChatMsg` — chat kind comes from the opcode.
static uint32 ReadCataChatLanguageField(WorldPacket &packet) {
  if (packet.GetReadPos() + sizeof(uint32) > packet.Size())
    return LANG_UNIVERSAL;
  return packet.Read<uint32>();
}

} // namespace

void WorldSession::HandleAddonMessageChat(WorldPacket &packet) {
  uint32 type = 0;
  if (!DecodeAddonChatOpcode(packet.GetOpcode(), type)) {
    LOG_DEBUG("HandleAddonMessageChat: unknown opcode 0x{:X}", packet.GetOpcode());
    return;
  }
  // Reference: `WorldSession::HandleAddonMessagechatOpcode` — different bit layout
  // (prefix lengths, broadcast to group/guild). Not required for baseline world
  // bring-up; consume nothing (payload is self-contained per TCP frame).
  (void)type;
  LOG_DEBUG("CMSG_MESSAGECHAT_ADDON_* (type {}) — addon channel not implemented",
            type);
}

void WorldSession::HandleMessageChat(WorldPacket &packet) {
  uint32 type = 0;
  uint32 lang = LANG_UNIVERSAL;
  if (!DecodeStandardChatOpcode(packet.GetOpcode(), type)) {
    LOG_DEBUG("HandleMessageChat: opcode 0x{:X} not a standard chat opcode",
              packet.GetOpcode());
    return;
  }

  if (type != CHAT_MSG_EMOTE && type != CHAT_MSG_AFK && type != CHAT_MSG_DND) {
    lang = ReadCataChatLanguageField(packet);
    // On the wire the language field is often serialized as int32; -1 is LANG_ADDON
    // (0xFFFFFFFF). Real addon messages use CMSG_MESSAGECHAT_ADDON_* for most
    // channels; for /say and /yell the 4.3.4 client still sends -1 as "default
    // speech" in some builds. If we treat that as addon here, `AddonLangAllowed`
    // fails and the player always sees "You may not speak that language."
    if (lang == CHAT_LANG_ADDON && !AddonLangAllowedForType(type))
      lang = LANG_UNIVERSAL;
  }

  std::string target;
  std::string channel;
  std::string message;

  BitReader br(packet);
  switch (type) {
  case CHAT_MSG_SAY:
  case CHAT_MSG_YELL:
  case CHAT_MSG_EMOTE:
  case CHAT_MSG_PARTY:
  case CHAT_MSG_GUILD:
  case CHAT_MSG_OFFICER:
  case CHAT_MSG_RAID:
  case CHAT_MSG_RAID_WARNING:
  case CHAT_MSG_BATTLEGROUND: {
    uint32 const textLen = br.ReadBits(9);
    message = br.ReadString(textLen);
    break;
  }
  case CHAT_MSG_WHISPER: {
    uint32 const nameLen = br.ReadBits(10);
    uint32 const textLen = br.ReadBits(9);
    target = br.ReadString(nameLen);
    message = br.ReadString(textLen);
    break;
  }
  case CHAT_MSG_CHANNEL: {
    uint32 const chLen = br.ReadBits(10);
    uint32 const textLen = br.ReadBits(9);
    message = br.ReadString(textLen);
    channel = br.ReadString(chLen);
    break;
  }
  case CHAT_MSG_AFK:
  case CHAT_MSG_DND: {
    uint32 const textLen = br.ReadBits(9);
    message = br.ReadString(textLen);
    break;
  }
  default:
    return;
  }

  if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND && message.empty())
    return;

  auto const chatPreview = [&message]() {
    if (message.size() <= 96)
      return message;
    return message.substr(0, 96) + "...";
  };
  LOG_DEBUG("[CHAT] in opcode=0x{:X} legacy={} type={} lang={} msgLen={} msg='{}'",
            packet.GetOpcode(), 0, type, lang,
            message.size(), chatPreview());
  LOG_DEBUG("[CHAT] knows lang={} => {}", lang,
            PlayerKnowsLanguage(_knownSpellIds, lang) ? 1 : 0);

  if (_commandService->IsCommand(message)) {
    _commandService->ExecuteCommand(
        std::static_pointer_cast<ICommandSession>(shared_from_this()), message);
    return;
  }

  if (_playerGuid == 0)
    return;

  if (type != CHAT_MSG_EMOTE && type != CHAT_MSG_AFK && type != CHAT_MSG_DND) {
    // Force deterministic spoken language for this character. Some clients can
    // keep stale language selections in the normal chat box and send invalid
    // language ids; using race default avoids client-side "cannot speak" spam.
    lang = DefaultLanguageForRace(_playerRace);
    if (lang == CHAT_LANG_ADDON) {
      if (!AddonLangAllowedForType(type)) {
        lang = DefaultLanguageForRace(_playerRace);
      }
    } else {
      // Keep permissive behavior: if spell list is out of sync, do not block chat.
      if (!PlayerKnowsLanguage(_knownSpellIds, lang))
        lang = DefaultLanguageForRace(_playerRace);
    }
  }

  // `receiverGuid` must be 0 for open channels (/say, /yell, party, guild, …).
  // Sending the sender GUID twice makes the 4.3.4 client mis-parse the packet
  // (language filter / scramble + "You cannot speak that language.").
  // TODO: set real target GUID for `CHAT_MSG_WHISPER` when name→guid exists.
  uint64 const receiverGuid = 0;

  bool gmChat = _gmAppearance.gmTagOn;
  std::string senderNameForGmPacket;
  if (gmChat) {
    if (auto ch = _charService->GetCharacterByGuid(_playerGuid))
      senderNameForGmPacket = ch->GetName();
    if (senderNameForGmPacket.empty())
      gmChat = false;
  }

  uint8 chatTag = static_cast<uint8>(CHAT_TAG_WIRE_NONE);
  if (_gmAppearance.dndOn)
    chatTag |= static_cast<uint8>(CHAT_TAG_WIRE_DND);
  if (_gmAppearance.devTagOn)
    chatTag |= static_cast<uint8>(CHAT_TAG_WIRE_DEV);
  if (gmChat)
    chatTag |= static_cast<uint8>(CHAT_TAG_WIRE_GM);

  WorldPacket response(
      static_cast<uint32>(gmChat ? SMSG_GM_MESSAGECHAT : SMSG_MESSAGECHAT),
      256 + message.size() + senderNameForGmPacket.size());
  std::string const *chPtr =
      (type == CHAT_MSG_CHANNEL && !channel.empty()) ? &channel : nullptr;
  AppendSmsgMessageChatPayload(response, type, lang, _playerGuid, receiverGuid,
                                message, chPtr, gmChat, senderNameForGmPacket,
                                chatTag);
  LOG_DEBUG("[CHAT] out type={} lang={} receiverGuid={} msgLen={}", type, lang,
            receiverGuid, message.size());
  SendPacket(response);

  if (type == CHAT_MSG_SAY || type == CHAT_MSG_YELL) {
    if (auto map = WorldService::Instance().GetMap(_mapId)) {
      map->BroadcastPacketToNearby(_playerGuid, response, false);
    }
  }
}

void WorldSession::SendNotification(const std::string &message) {
  WorldPacket response(static_cast<uint32>(SMSG_MESSAGECHAT), 128 + message.size());
  AppendSmsgMessageChatPayload(response, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, 0, 0,
                               message, nullptr, false, "", CHAT_TAG_WIRE_NONE);
  SendPacket(response);
}

void WorldSession::SendScreenNotification(std::string const &message) {
  if (message.empty())
    return;
  WorldPacket pkt(static_cast<uint32>(SMSG_NOTIFICATION), 32 + message.size());
  BitWriter bw(pkt);
  bw.WriteBits(static_cast<uint32>(message.size()), 13);
  bw.Flush();
  pkt.WriteStringNoNull(message);
  SendPacket(pkt);
}

void WorldSession::RequestDisconnect(std::string const &reason) {
  std::string const msg =
      reason.empty() ? std::string("You have been disconnected by a game master.")
                       : (std::string("Disconnecting: ") + reason);
  SendNotification(msg);
  Close();
}

} // namespace Firelands
