#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/Logger.h>
#include <shared/Crypto.h>
#include <random>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Firelands {

    WorldSession::WorldSession(tcp::socket socket, std::shared_ptr<AuthService> authService, std::shared_ptr<CharacterService> charService)
        : _socket(std::move(socket)), _authService(std::move(authService)), _charService(std::move(charService)), _accountId(0) {
    }

    void WorldSession::Start() {
        LOG_INFO("WorldSession started for {}", GetIpAddress());
        
        // Generate Server Seed
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32> dis(0, 0xFFFFFFFF);
        _serverSeed = dis(gen);

        SendAuthChallenge();
        DoRead();
    }

    void WorldSession::SendPacket(WorldPacket& packet) {
        ByteBuffer buffer;
        // In Cataclysm, Server header is [Size:2 (BE)][Opcode:2 (LE)]
        uint16 size = static_cast<uint16>(packet.Size() + 2);
        buffer.Append<uint8>((size >> 8) & 0xFF);
        buffer.Append<uint8>(size & 0xFF);
        buffer.Append<uint16>(static_cast<uint16>(packet.GetOpcode()));
        buffer.Append(packet.GetBuffer(), packet.Size());

        SendPacket(buffer);
    }

    void WorldSession::SendPacket(ByteBuffer& buffer) {
        auto self(shared_from_this());
        boost::asio::async_write(_socket, boost::asio::buffer(buffer.GetBuffer(), buffer.Size()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    Close();
                }
            });
    }

    void WorldSession::SendAuthChallenge() {
        WorldPacket packet(SMSG_AUTH_CHALLENGE);
        // In 4.3.4.15595 SMSG_AUTH_CHALLENGE: [uint8[32] dos_challenge][uint32 seed][uint8 flag]
        // Reference: WorldPackets::Auth::AuthChallenge::Write()
        
        // Append 32 bytes of zeros for DOS challenge (simplified initialization)
        for (int i = 0; i < 32; ++i) packet.Append<uint8>(0);
        
        // Append actual Server Seed (Challenge)
        packet.Append<uint32>(_serverSeed);
        
        // Append DosZeroBits (flag)
        packet.Append<uint8>(1);

        SendPacket(packet);
        LOG_INFO("SMSG_AUTH_CHALLENGE sent (seed: 0x{:08X})", _serverSeed);
    }

    void WorldSession::Close() {
        _socket.close();
    }

    std::string WorldSession::GetIpAddress() const {
        try {
            return _socket.remote_endpoint().address().to_string();
        } catch (...) {
            return "unknown";
        }
    }

    void WorldSession::DoRead() {
        auto self(shared_from_this());
        _socket.async_read_some(boost::asio::buffer(_readBuffer, 1024),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    ByteBuffer buffer;
                    buffer.Append(_readBuffer, length);
                    HandlePacket(buffer);
                    DoRead();
                } else if (ec != boost::asio::error::operation_aborted) {
                    Close();
                }
            });
    }

    void WorldSession::HandlePacket(ByteBuffer& buffer) {
        if (buffer.Size() < 4) return;

        // In WoW 4.3.4, Client header is [Size:2 (BE)][Opcode:4 (LE)]
        uint16 size = buffer.Read<uint16>();
        size = (size << 8) | (size >> 8); 

        uint32 opcode = buffer.Read<uint32>();
        
        WorldPacket packet(opcode, size);
        if (size > 0) {
            // The size actually includes the opcode (4 bytes), so payload size is size - 4
            // Wait, usually size is payload size + opcode size.
            // In 4.3.4, size is indeed payload + opcode size.
            // But let's check the buffer.
            packet.Append(buffer.GetBuffer() + 6, buffer.Size() - 6);
        }

        ProcessPacket(packet);
    }

    void WorldSession::ProcessPacket(WorldPacket& packet) {
        uint32 opcode = packet.GetOpcode();
        LOG_INFO("WorldSession received packet: {}, size: {}", packet.GetOpcodeName(), packet.Size());

        switch (opcode) {
            case CMSG_AUTH_SESSION:
                HandleAuthSession(packet);
                break;
            case CMSG_CHAR_ENUM:
                HandleCharEnum(packet);
                break;
            case CMSG_CHAR_CREATE:
                HandleCharCreate(packet);
                break;
            case CMSG_CHAR_DELETE:
                HandleCharDelete(packet);
                break;
            case CMSG_PLAYER_LOGIN:
                HandlePlayerLogin(packet);
                break;
            case CMSG_LOG_DISCONNECT:
                LOG_INFO("Client disconnected (CMSG_LOG_DISCONNECT)");
                Close();
                break;
            default:
                LOG_WARN("Unknown or unhandled world opcode: 0x{:04X}", opcode);
                break;
        }
    }

    void WorldSession::HandleAuthSession(WorldPacket& packet) {
        // Cata 4.3.4 CMSG_AUTH_SESSION is extremely scattered.
        // We must follow the order in WorldPackets::Auth::AuthSession::Read() exactly.
        
        uint32 loginServerId = packet.Read<uint32>();
        uint32 battlegroupId = packet.Read<uint32>();
        uint32 loginServerType = packet.Read<uint32>();
        
        uint8 digest[20];
        digest[10] = packet.Read<uint8>();
        digest[18] = packet.Read<uint8>();
        digest[12] = packet.Read<uint8>();
        digest[5] = packet.Read<uint8>();
        
        uint64 dosResponse = packet.Read<uint64>();
        
        digest[15] = packet.Read<uint8>();
        digest[9] = packet.Read<uint8>();
        digest[19] = packet.Read<uint8>();
        digest[4] = packet.Read<uint8>();
        digest[7] = packet.Read<uint8>();
        digest[16] = packet.Read<uint8>();
        digest[3] = packet.Read<uint8>();
        
        uint32 build = packet.Read<uint32>();
        digest[8] = packet.Read<uint8>();
        
        uint32 realmId = packet.Read<uint32>();
        uint32 buildType = packet.Read<uint32>();
        
        digest[17] = packet.Read<uint8>();
        digest[6] = packet.Read<uint8>();
        digest[0] = packet.Read<uint8>();
        digest[1] = packet.Read<uint8>();
        digest[11] = packet.Read<uint8>();
        
        uint32 localChallenge = packet.Read<uint32>();
        digest[2] = packet.Read<uint8>();
        uint32 regionId = packet.Read<uint32>();
        
        digest[14] = packet.Read<uint8>();
        digest[13] = packet.Read<uint8>();
        
        uint32 addonDataSize = packet.Read<uint32>();
        if (addonDataSize > 0) {
            // Skip addon data for now
            for (uint32 i = 0; i < addonDataSize; ++i) packet.Read<uint8>();
        }
        
        // Bit-packed fields at the end
        // UseIPv6 (1), AccountNameLength (12), AccountName (String)
        
        // Logic for bit reading (manual version)
        uint8 firstBitByte = packet.Read<uint8>();
        bool useIPv6 = (firstBitByte & 0x01) != 0;
        uint16 nameLen = (firstBitByte >> 1) | (static_cast<uint16>(packet.Read<uint8>()) << 7);
        nameLen &= 0x0FFF; // 12 bits

        std::string account;
        for (uint16 i = 0; i < nameLen; ++i) {
            account += static_cast<char>(packet.Read<uint8>());
        }

        LOG_INFO("CMSG_AUTH_SESSION: Account: {}, Build: {}, RealmID: {}", account, build, realmId);
        
        // 1. Find account to get ID
        auto accountOpt = _authService->FindAccount(account);
        if (!accountOpt) {
            LOG_ERROR("CMSG_AUTH_SESSION: Account '{}' not found in database.", account);
            Close();
            return;
        }

        // 2. Get saved Session Key K from database
        std::vector<uint8_t> K = _authService->GetSessionKey(accountOpt->id);
        if (K.empty()) {
            LOG_ERROR("CMSG_AUTH_SESSION: No session key K found for account '{}'.", account);
            Close();
            return;
        }

        LOG_INFO("CMSG_AUTH_SESSION: Retrieved session key K for {}, length: {}", account, K.size());

        // 3. Perform Digest validation
        // Cata 4.3.4: Digest = SHA1(Account, LoginServerID, LocalChallenge, ServerSeed, SessionKey K)
        // Reference: AuthHandler.cpp in firelands-cata
        Crypto::SHA1 sha;
        sha.Update(Crypto::ToUpper(account));
        sha.Update<uint32>(0); // t = 0 in reference
        sha.Update<uint32>(localChallenge);
        sha.Update<uint32>(_serverSeed);
        sha.Update(K);
        auto calculatedDigest = sha.Finalize();

        if (std::memcmp(calculatedDigest.data(), digest, 20) != 0) {
            LOG_ERROR("CMSG_AUTH_SESSION: Digest validation failed for account '{}'!", account);
            Close();
            return;
        }

        _accountId = accountOpt->id;
        LOG_INFO("CMSG_AUTH_SESSION: Digest validated successfully for account '{}' (ID: {}).", account, _accountId);

        // 4. Send SMSG_AUTH_RESPONSE
        WorldPacket response(SMSG_AUTH_RESPONSE);
        // In 4.3.4 SMSG_AUTH_RESPONSE: [Bit: HasWaitInfo][Bit: HasSuccessInfo][Flush][SuccessData...][uint8 Result][WaitData...]
        // SuccessData: [uint32 TimeRemain][uint8 ActiveExp][uint32 PCKick][uint8 AccountExp][uint32 TimeRested][uint8 TimeOptions]
        
        response.Append<uint8>(0x02); // Bits: HasWaitInfo=0, HasSuccessInfo=1 (binary 00000010)
        
        // SuccessInfo Data
        response.Append<uint32>(0);    // TimeRemain
        response.Append<uint8>(3);     // ActiveExpansion (3 = Cataclysm)
        response.Append<uint32>(0);    // TimeSecondsUntilPCKick
        response.Append<uint8>(3);     // AccountExpansion (3 = Cataclysm)
        response.Append<uint32>(0);    // TimeRested
        response.Append<uint8>(0);    // TimeOptions (0 = None)
        
        response.Append<uint8>(0x0C); // Result (0x0C = AUTH_OK)

        SendPacket(response);
        LOG_INFO("SMSG_AUTH_RESPONSE sent: AUTH_OK");
    }

    void WorldSession::HandleCharEnum(WorldPacket& /*packet*/) {
        LOG_INFO("WorldSession::HandleCharEnum called for account ID: {}", _accountId);
        auto characters = _charService->GetCharactersForAccount(_accountId);

        WorldPacket response(SMSG_CHAR_ENUM);
        // In 4.3.4 SMSG_CHAR_ENUM is bit-packed.
        // Extremely simplified layout for 0 characters (common case for fresh accounts):
        // [1 bit: Unk][21 bits: Count][... rest of characters]
        
        uint32 count = static_cast<uint32>(characters.size());
        
        // I'll implement a proper (but minimal) BitStream logic here for the count.
        // Count is 21 bits.
        uint32 packedCount = count; 
        response.Append<uint8>(0); // Unk bit
        response.Append<uint8>(packedCount & 0xFF);
        response.Append<uint8>((packedCount >> 8) & 0xFF);
        response.Append<uint8>((packedCount >> 16) & 0xFF);
        
        // For each character, there's a lot of data.
        // If count > 0, we'd append them here.
        
        SendPacket(response);
        LOG_INFO("SMSG_CHAR_ENUM sent with {} characters", count);
    }

    void WorldSession::HandleCharCreate(WorldPacket& packet) {
        std::string name = packet.ReadString();
        uint8 race = packet.Read<uint8>();
        uint8 klass = packet.Read<uint8>();
        uint8 gender = packet.Read<uint8>();
        uint8 skin = packet.Read<uint8>();
        uint8 face = packet.Read<uint8>();
        uint8 hairStyle = packet.Read<uint8>();
        uint8 hairColor = packet.Read<uint8>();
        uint8 facialHair = packet.Read<uint8>();
        uint8 outfitId = packet.Read<uint8>();

        LOG_INFO("CMSG_CHAR_CREATE for '{}' (Race: {}, Class: {})", name, race, klass);

        bool success = _charService->CreateCharacter(_accountId, name, race, klass, gender, skin, face, hairStyle, hairColor, facialHair);

        WorldPacket response(SMSG_CHAR_CREATE);
        // 4.3.4 SMSG_CHAR_CREATE Response
        response.Append<uint8>(success ? 0x31 : 0x32); // CHAR_CREATE_SUCCESS=0x31, CHAR_CREATE_ERROR=0x32 (Cata 4.3.4)

        SendPacket(response);
        LOG_INFO("SMSG_CHAR_CREATE sent result: {}", success ? "SUCCESS" : "FAIL");
    }

    void WorldSession::HandleCharDelete(WorldPacket& packet) {
        uint64 guid = packet.Read<uint64>();

        LOG_INFO("CMSG_CHAR_DELETE for GUID: {}", guid);

        bool success = _charService->DeleteCharacter(static_cast<uint32>(guid), _accountId);

        WorldPacket response(SMSG_CHAR_DELETE);
        // 4.3.4 SMSG_CHAR_DELETE Response
        // Success = 0x47, Error = 0x48 (Legacy but usually works)
        response.Append<uint8>(success ? 0x47 : 0x48); 

        SendPacket(response);
        LOG_INFO("SMSG_CHAR_DELETE sent result: {}", success ? "SUCCESS" : "FAIL");
    }

    void WorldSession::HandlePlayerLogin(WorldPacket& packet) {
        uint64 guid = packet.Read<uint64>();
        LOG_INFO("CMSG_PLAYER_LOGIN for GUID: {}", guid);

        // 1. Send SMSG_LOGIN_VERIFY_WORLD
        WorldPacket verify(SMSG_LOGIN_VERIFY_WORLD);
        verify.Append<uint32>(0);      // Map ID (0 = Azeroth)
        verify.Append<float>(0.0f);    // X
        verify.Append<float>(0.0f);    // Y
        verify.Append<float>(0.0f);    // Z
        verify.Append<float>(0.0f);    // O
        SendPacket(verify);

        // 2. Send SMSG_TUTORIAL_FLAGS (all zero)
        WorldPacket tutorials(SMSG_TUTORIAL_FLAGS);
        for (int i = 0; i < 8; ++i) tutorials.Append<uint32>(0);
        SendPacket(tutorials);

        // 3. Send SMSG_TIME_SYNC_REQ
        WorldPacket timeSync(SMSG_TIME_SYNC_REQ);
        timeSync.Append<uint32>(0); // Counter
        SendPacket(timeSync);

        SendInitialObjectUpdate(guid);

        // 4. Send SMSG_ACCOUNT_DATA_TIMES
        WorldPacket accountData(SMSG_ACCOUNT_DATA_TIMES);
        accountData.Append<uint32>(time(nullptr)); 
        accountData.Append<uint8>(1); // Unk
        accountData.Append<uint32>(0); // Mask
        SendPacket(accountData);

        // 5. Send SMSG_MOTD
        WorldPacket motd(SMSG_MOTD);
        motd.Append<uint32>(1); // Line count
        motd.Append("Welcome to Firelands Emulator (Cataclysm 4.3.4)");
        SendPacket(motd);

        LOG_INFO("Handshake for Player Login completed for GUID: {}", guid);
    }

    void WorldSession::WritePackedGuid(uint64 guid, BitWriter& bw, ByteBuffer& bb) {
        // Simple bitmask-led packing for 4.x
        uint8 mask = 0;
        uint8 bytes[8];
        uint8 count = 0;
        
        for (int i = 0; i < 8; ++i) {
            uint8 b = (guid >> (i * 8)) & 0xFF;
            if (b) {
                mask |= (1 << i);
                bytes[count++] = b;
            }
        }
        
        bb.Append<uint8>(mask);
        for (int i = 0; i < count; ++i) {
            bb.Append<uint8>(bytes[i]);
        }
    }

    void WorldSession::SendInitialObjectUpdate(uint64 guid) {
        WorldPacket packet(SMSG_UPDATE_OBJECT);
        packet.Append<uint32>(1); // Number of updates
        packet.Append<uint8>(2);  // UPDATETYPE_CREATE_OBJECT (Active Player)

        BitWriter bw(packet);
        WritePackedGuid(guid, bw, packet);
        
        packet.Append<uint8>(4); // TYPEID_PLAYER

        // Movement Data Bits
        bw.WriteBit(true);  // Has movement data
        bw.WriteBit(false); // No transport
        bw.WriteBit(true);  // Is Self
        bw.WriteBit(false); // No stationary
        bw.WriteBit(false); // No animkits
        bw.Flush();

        // Movement Data Bytes
        packet.Append<uint32>(0);    // Movement Flags
        packet.Append<uint16>(0);    // Movement Flags 2
        packet.Append<uint32>(0);    // Time
        packet.Append<float>(0.0f);  // X
        packet.Append<float>(0.0f);  // Y
        packet.Append<float>(0.0f);  // Z
        packet.Append<float>(0.0f);  // Orientation
        packet.Append<uint32>(0);    // Fall time

        // Other bit-packed data (simplified)
        bw.WriteBit(false); // No victim
        bw.WriteBit(false); // No vehicle
        bw.WriteBit(false); // No pvp guid
        bw.Flush();

        // Update Fields (Zero Mask = No fields sent)
        packet.Append<uint32>(0); 

        SendPacket(packet);
        LOG_INFO("SMSG_UPDATE_OBJECT sent (Player Spawned)");
    }

} // namespace Firelands
