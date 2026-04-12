#ifndef FIRELANDS_SHARED_NETWORK_AUTH_PACKETS_H
#define FIRELANDS_SHARED_NETWORK_AUTH_PACKETS_H

#include <shared/Common.h>
#include <shared/network/ByteBuffer.h>
#include <vector>
#include <string>

namespace Firelands {

    enum AuthOpcode : uint8 {
        AUTH_LOGON_CHALLENGE = 0x00,
        AUTH_LOGON_PROOF     = 0x01,
        // ... more as needed
    };

    struct AuthLogonChallenge_C {
        uint8 protocol;
        uint16 size;
        char gameName[4];
        uint8 versionMajor;
        uint8 versionMinor;
        uint8 versionPatch;
        uint16 build;
        char platform[4];
        char os[4];
        char locale[4];
        uint32 timezone;
        uint32 ip;
        std::string username;

        void Read(ByteBuffer& buffer) {
            // Opcode was already read to determine this packet type
            protocol = buffer.Read<uint8>();
            size = buffer.Read<uint16>();
            for (int i = 0; i < 4; ++i) gameName[i] = buffer.Read<char>();
            versionMajor = buffer.Read<uint8>();
            versionMinor = buffer.Read<uint8>();
            versionPatch = buffer.Read<uint8>();
            build = buffer.Read<uint16>();
            for (int i = 0; i < 4; ++i) platform[i] = buffer.Read<char>();
            for (int i = 0; i < 4; ++i) os[i] = buffer.Read<char>();
            for (int i = 0; i < 4; ++i) locale[i] = buffer.Read<char>();
            timezone = buffer.Read<uint32>();
            ip = buffer.Read<uint32>();
            uint8 usernameLen = buffer.Read<uint8>();
            username = "";
            for (int i = 0; i < usernameLen; ++i) username += buffer.Read<char>();
        }
    };

    enum AuthResult : uint8 {
        AUTH_SUCCESS             = 0x00,
        AUTH_FAIL_UNKNOWN0       = 0x01,
        AUTH_FAIL_UNKNOWN1       = 0x02,
        AUTH_FAIL_BANNED         = 0x03,
        AUTH_FAIL_UNKNOWN_ACCOUNT = 0x04,
        AUTH_FAIL_WRONG_PASSWORD = 0x05,
        AUTH_FAIL_ALREADY_LOGGED  = 0x06,
        AUTH_FAIL_NO_TIME        = 0x07,
        AUTH_FAIL_DB_BUSY        = 0x08,
        AUTH_FAIL_VERSION_INVALID = 0x09,
        AUTH_FAIL_DOWNLOAD_FILE  = 0x0A,
        AUTH_FAIL_INVALID_SERVER = 0x0B,
        AUTH_FAIL_SUSPENDED      = 0x0C,
        AUTH_FAIL_NOT_ALLOWED    = 0x0D,
        AUTH_FAIL_ALREADY_LOGGED2 = 0x0E,
        AUTH_FAIL_ALREADY_LOGGED3 = 0x0F,
    };

    struct AuthLogonChallenge_S {
        uint8 opcode; // 0x00
        uint8 result; // AuthResult
        // If result is AUTH_SUCCESS:
        std::vector<uint8> B; // 32 bytes
        uint8 gLen; // 1 byte
        uint8 g; // g bytes (usually 1 byte)
        uint8 NLen; // 1 byte
        std::vector<uint8> N; // N bytes (usually 32 bytes)
        std::vector<uint8> salt; // 32 bytes
        std::vector<uint8> unk3; // 16 bytes
        uint8 securityFlags; // 1 byte

        void Write(ByteBuffer& buffer) const {
            buffer.Append<uint8>(opcode);
            buffer.Append<uint8>(0x00); // unk
            buffer.Append<uint8>(result);
            if (result == AUTH_SUCCESS) {
                buffer.Append(B.data(), B.size());
                buffer.Append<uint8>(gLen);
                buffer.Append<uint8>(g);
                buffer.Append<uint8>(NLen);
                buffer.Append(N.data(), N.size());
                buffer.Append(salt.data(), salt.size());
                buffer.Append(unk3.data(), unk3.size());
                buffer.Append<uint8>(securityFlags);
            }
        }
    };

    struct AuthLogonProof_C {
        uint8 A[32];
        uint8 M1[20];
        uint8 crc_hash[20];
        uint8 number_of_keys;
        uint8 securityFlags;

        void Read(ByteBuffer& buffer) {
            for (int i = 0; i < 32; ++i) A[i] = buffer.Read<uint8>();
            for (int i = 0; i < 20; ++i) M1[i] = buffer.Read<uint8>();
            for (int i = 0; i < 20; ++i) crc_hash[i] = buffer.Read<uint8>();
            number_of_keys = buffer.Read<uint8>();
            securityFlags = buffer.Read<uint8>();
        }
    };

    struct AuthLogonProof_S {
        uint8 opcode; // 0x01
        uint8 result; // AuthResult
        uint8 M2[20];
        uint32 account_flags;
        uint32 survey_id;
        uint16 login_flags;

        void Write(ByteBuffer& buffer) const {
            buffer.Append<uint8>(opcode);
            buffer.Append<uint8>(result);
            if (result == AUTH_SUCCESS) {
                buffer.Append(M2, 20);
                buffer.Append<uint32>(account_flags);
                buffer.Append<uint32>(survey_id);
                buffer.Append<uint16>(login_flags);
            }
        }
    };

} // namespace Firelands

#endif // FIRELANDS_SHARED_NETWORK_AUTH_PACKETS_H
