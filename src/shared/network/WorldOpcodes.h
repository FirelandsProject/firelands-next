#ifndef FIRELANDS_SHARED_NETWORK_WORLD_OPCODES_H
#define FIRELANDS_SHARED_NETWORK_WORLD_OPCODES_H

#include <shared/Common.h>

namespace Firelands {

    enum WorldOpcode : uint32 {
        SMSG_AUTH_CHALLENGE              = 0x4542,
        CMSG_AUTH_SESSION                = 0x0449,
        SMSG_AUTH_RESPONSE               = 0x5DB6,

        CMSG_CHAR_ENUM                   = 0x3286,
        SMSG_CHAR_ENUM                   = 0x33A6,
        CMSG_CHAR_CREATE                 = 0x30A7,
        SMSG_CHAR_CREATE                 = 0x30A5,
        CMSG_CHAR_DELETE                 = 0x34A6,
        SMSG_CHAR_DELETE                 = 0x31A4,

        CMSG_PLAYER_LOGIN                = 0x3685,
        SMSG_LOGIN_VERIFY_WORLD          = 0x3DA5,
        SMSG_TIME_SYNC_REQ               = 0x3486,
        SMSG_TUTORIAL_FLAGS              = 0x3FA4,
        SMSG_MOTD                        = 0x3785,
        SMSG_UPDATE_OBJECT               = 0x34A4,
        SMSG_ACCOUNT_DATA_TIMES          = 0x3184,
        SMSG_INITIAL_SPELLS              = 0x3384,
        SMSG_ACTION_BUTTONS              = 0x3FA5,

        CMSG_LOG_DISCONNECT              = 0x446D,
    };

} // namespace Firelands

#endif
