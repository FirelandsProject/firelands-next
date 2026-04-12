#include <gtest/gtest.h>
#include <shared/network/AuthPackets.h>

using namespace Firelands;

TEST(AuthPacketsTest, AuthLogonChallenge_C_Read) {
    ByteBuffer buffer;
    buffer.Append<uint8>(3); // protocol
    buffer.Append<uint16>(30); // size
    buffer.Append((const uint8*)"WoW", 4); // gameName
    buffer.Append<uint8>(4); // major
    buffer.Append<uint8>(3); // minor
    buffer.Append<uint8>(4); // patch
    buffer.Append<uint16>(15595); // build
    buffer.Append((const uint8*)"x86", 4); // platform
    buffer.Append((const uint8*)"OSX", 4); // os
    buffer.Append((const uint8*)"enUS", 4); // locale
    buffer.Append<uint32>(60); // timezone
    buffer.Append<uint32>(0x7F000001); // ip
    buffer.Append<uint8>(4); // username len
    buffer.Append((const uint8*)"TEST", 4); // username

    AuthLogonChallenge_C packet;
    packet.Read(buffer);

    EXPECT_EQ(packet.protocol, 3);
    EXPECT_EQ(packet.build, 15595);
    EXPECT_EQ(packet.username, "TEST");
}

TEST(AuthPacketsTest, AuthLogonChallenge_S_Write) {
    AuthLogonChallenge_S packet;
    packet.opcode = AUTH_LOGON_CHALLENGE;
    packet.result = AUTH_SUCCESS;
    packet.B.assign(32, 0xBB);
    packet.gLen = 1;
    packet.g = 7;
    packet.NLen = 32;
    packet.N.assign(32, 0x11);
    packet.salt.assign(32, 0x22);
    packet.unk3.assign(16, 0);
    packet.securityFlags = 0;

    ByteBuffer buffer;
    packet.Write(buffer);

    // Opcode + unk + result + B(32) + gLen + g + NLen + N(32) + s(32) + unk(16) + flags(1)
    // 1 + 1 + 1 + 32 + 1 + 1 + 1 + 32 + 32 + 16 + 1 = 120 bytes
    EXPECT_EQ(buffer.Size(), 120);
    
    EXPECT_EQ(buffer.Data()[0], 0x00); // opcode
    EXPECT_EQ(buffer.Data()[2], 0x00); // result (AUTH_SUCCESS)
    EXPECT_EQ(buffer.Data()[3], 0xBB); // First byte of B
}
