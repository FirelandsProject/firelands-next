#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/Logger.h>

namespace Firelands {

void WorldSession::ProcessPacket(WorldPacket &packet) {
  uint32 opcode = packet.GetOpcode();
  LOG_DEBUG("[CMSG] {} payload={} crypt={}", packet.GetOpcodeName(),
            packet.Size(), _crypt.IsInitialized());

  switch (opcode) {
  case CMSG_AUTH_SESSION:
    HandleAuthSession(packet);
    break;
  case CMSG_CHAR_CREATE:
    HandleCharCreate(packet);
    break;
  case CMSG_CHAR_DELETE:
    HandleCharDelete(packet);
    break;
  case CMSG_CHAR_ENUM:
    HandleCharEnum(packet);
    break;
  case MSG_QUERY_NEXT_MAIL_TIME:
    HandleQueryNextMailTime(packet);
    break;
  case CMSG_MAIL_GET_LIST:
    HandleMailGetList(packet);
    break;
  case CMSG_CALENDAR_GET_NUM_PENDING:
    HandleCalendarGetNumPending(packet);
    break;
  case CMSG_ZONEUPDATE:
    HandleZoneUpdate(packet);
    break;
  case CMSG_GM_TICKET_CREATE:
    HandleGmTicketCreate(packet);
    break;
  case CMSG_GM_TICKET_UPDATE_TEXT:
    HandleGmTicketUpdateText(packet);
    break;
  case CMSG_GM_TICKET_DELETE_TICKET:
    HandleGmTicketDelete(packet);
    break;
  case CMSG_GM_TICKET_GET_TICKET:
    HandleGmTicketGetTicket(packet);
    break;
  case CMSG_GM_TICKET_GET_SYSTEM_STATUS:
    HandleGmTicketSystemStatus(packet);
    break;
  case CMSG_GM_TICKET_RESPONSE_RESOLVE:
    HandleGmTicketResponseResolve(packet);
    break;
  case CMSG_GM_SURVEY_SUBMIT:
    // Optional post-resolution survey; not implemented.
    break;
  case CMSG_SET_ACTIVE_MOVER:
  case CMSG_SET_ACTIONBAR_TOGGLES:
  case CMSG_REQUEST_RAID_INFO:
  case CMSG_UNREGISTER_ALL_ADDON_PREFIXES:
  case CMSG_BATTLEFIELD_STATUS:
  case CMSG_QUERY_BATTLEFIELD_STATE:
  case CMSG_JOIN_CHANNEL:
  case CMSG_CONTACT_LIST:
  case CMSG_SAVE_CUF_PROFILES:
  case CMSG_VOICE_SESSION_ENABLE:
  case CMSG_GUILD_SET_ACHIEVEMENT_TRACKING:
  case CMSG_REQUEST_CATEGORY_COOLDOWNS:
  case CMSG_WORLD_STATE_UI_TIMER_UPDATE:
    // Client probes features we haven't implemented yet. For stability we safely
    // ignore these requests (no side effects, no disconnect).
    break;
  case CMSG_LFG_GET_STATUS:
    HandleLfgGetStatus(packet);
    break;
  case CMSG_LFG_LOCK_INFO_REQUEST:
    HandleLfgLockInfoRequest(packet);
    break;
  case CMSG_GUILD_BANK_REMAINING_WITHDRAW_MONEY_QUERY:
    HandleGuildBankRemainingWithdrawMoneyQuery(packet);
    break;
  case CMSG_REQUEST_CEMETERY_LIST:
    HandleRequestCemeteryList(packet);
    break;
  case CMSG_LOADING_SCREEN_NOTIFY:
    // Simply acknowledge loading screen progress
    break;
  case CMSG_LOG_DISCONNECT:
    Close();
    break;
  case CMSG_MESSAGECHAT_SAY:
  case CMSG_MESSAGECHAT_YELL:
  case CMSG_MESSAGECHAT_CHANNEL:
  case CMSG_MESSAGECHAT_WHISPER:
  case CMSG_MESSAGECHAT_GUILD:
  case CMSG_MESSAGECHAT_OFFICER:
  case CMSG_MESSAGECHAT_AFK:
  case CMSG_MESSAGECHAT_DND:
  case CMSG_MESSAGECHAT_EMOTE:
  case CMSG_MESSAGECHAT_PARTY:
  case CMSG_MESSAGECHAT_RAID:
  case CMSG_MESSAGECHAT_BATTLEGROUND:
  case CMSG_MESSAGECHAT_RAID_WARNING:
    HandleMessageChat(packet);
    break;
  case CMSG_MESSAGECHAT_ADDON_BATTLEGROUND:
  case CMSG_MESSAGECHAT_ADDON_GUILD:
  case CMSG_MESSAGECHAT_ADDON_OFFICER:
  case CMSG_MESSAGECHAT_ADDON_PARTY:
  case CMSG_MESSAGECHAT_ADDON_RAID:
  case CMSG_MESSAGECHAT_ADDON_WHISPER:
    HandleAddonMessageChat(packet);
    break;
  case CMSG_CAST_SPELL:
    HandleCastSpell(packet);
    break;
  case CMSG_GOSSIP_HELLO:
    HandleGossipHello(packet);
    break;
  case CMSG_GOSSIP_SELECT_OPTION:
    HandleGossipSelectOption(packet);
    break;
  case CMSG_NAME_QUERY:
    HandleNameQuery(packet);
    break;
  case CMSG_CREATURE_QUERY:
    HandleCreatureQuery(packet);
    break;
  case CMSG_QUERY_TIME:
    HandleQueryTime(packet);
    break;
  case CMSG_PLAYED_TIME:
    HandlePlayedTime(packet);
    break;
  case CMSG_MOVE_TIME_SKIPPED:
    HandleMoveTimeSkipped(packet);
    break;
  case CMSG_SET_SELECTION:
    HandleSetSelection(packet);
    break;
  case CMSG_AREA_TRIGGER:
  case CMSG_STAND_STATE_CHANGE:
  case CMSG_SET_SHEATHED:
    // Target selection updates are client-side/UI-only for now.
    break;
  case CMSG_PING:
    HandlePing(packet);
    break;
  case CMSG_PLAYER_LOGIN:
    HandlePlayerLogin(packet);
    break;
  case CMSG_LOGOUT_REQUEST:
    HandleLogoutRequest(packet);
    break;
  case CMSG_LOGOUT_CANCEL:
    HandleLogoutCancel(packet);
    break;
  case CMSG_READY_FOR_ACCOUNT_DATA_TIMES:
    HandleReadyForAccountDataTimes(packet);
    break;
  case CMSG_REQUEST_ACCOUNT_DATA:
    HandleRequestAccountData(packet);
    break;
  case CMSG_REALM_SPLIT:
    HandleRealmSplit(packet);
    break;
  case CMSG_TIME_SYNC_RESP:
    HandleTimeSyncResp(packet);
    break;
  case CMSG_UPDATE_ACCOUNT_DATA:
    HandleUpdateAccountData(packet);
    break;
  case CMSG_SWAP_INV_ITEM:
    HandleSwapInvItem(packet);
    break;
  case CMSG_SWAP_ITEM:
    HandleSwapItem(packet);
    break;
  case CMSG_DB_QUERY_BULK:
    HandleDbQueryBulk(packet);
    break;
  case CMSG_AUTO_EQUIP_ITEM:
    HandleAutoEquipItem(packet);
    break;
  case CMSG_AUTO_EQUIP_ITEM_SLOT:
    HandleAutoEquipItemSlot(packet);
    break;
  case CMSG_USE_ITEM:
    HandleUseItem(packet);
    break;
  case CMSG_DESTROY_ITEM:
    HandleDestroyItem(packet);
    break;
  case CMSG_CANCEL_TRADE:
    // Client sends this opportunistically (e.g. UI cleanup on login). Safe no-op.
    break;
  case CMSG_VIOLENCE_LEVEL:
    // Ignore violence level settings
    break;
  case CMSG_TUTORIAL_FLAG:
    HandleTutorialFlag(packet);
    break;
  case CMSG_TUTORIAL_CLEAR:
    HandleTutorialClear(packet);
    break;
  case CMSG_TUTORIAL_RESET:
    HandleTutorialReset(packet);
    break;
  case CMSG_COMPLETE_MOVIE:
    HandleCompleteMovie(packet);
    break;
  case CMSG_OPENING_CINEMATIC:
    HandleOpeningCinematic(packet);
    break;
  case CMSG_COMPLETE_CINEMATIC:
    HandleCompleteCinematic(packet);
    break;
  case CMSG_NEXT_CINEMATIC_CAMERA:
    HandleNextCinematicCamera(packet);
    break;
  case MSG_MOVE_HEARTBEAT:
  case MSG_MOVE_START_FORWARD:
  case MSG_MOVE_START_BACKWARD:
  case MSG_MOVE_START_STRAFE_LEFT:
  case MSG_MOVE_START_STRAFE_RIGHT:
  case MSG_MOVE_STOP:
  case MSG_MOVE_STOP_STRAFE:
  case MSG_MOVE_START_ASCEND:
  case MSG_MOVE_START_DESCEND:
  case MSG_MOVE_STOP_ASCEND:
  case MSG_MOVE_START_TURN_LEFT:
  case MSG_MOVE_START_TURN_RIGHT:
  case MSG_MOVE_STOP_TURN:
  case MSG_MOVE_START_PITCH_UP:
  case MSG_MOVE_START_PITCH_DOWN:
  case MSG_MOVE_STOP_PITCH:
  case MSG_MOVE_SET_RUN_MODE:
  case MSG_MOVE_SET_WALK_MODE:
  case MSG_MOVE_START_SWIM:
  case MSG_MOVE_STOP_SWIM:
  case MSG_MOVE_JUMP:
  case MSG_MOVE_SET_FACING:
  case MSG_MOVE_FALL_LAND:
    HandleMovement(packet);
    break;
  case MSG_MOVE_TELEPORT_ACK:
    HandleMoveTeleportAck(packet);
    break;
  case CMSG_MOVE_SET_CAN_FLY:
  case CMSG_MOVE_SET_CAN_FLY_ACK:
    // Client state after `SMSG_MOVE_SET_CAN_FLY`; echo raw payload like other
    // movement packets (decoder not wired — position stays on heartbeat).
    HandleMovement(packet);
    break;
  default:
    LOG_DEBUG("[PACKET] Unknown/unhandled opcode: 0x{:04X} (size: {})", opcode,
              packet.Size());
    break;
  }
}

} // namespace Firelands
