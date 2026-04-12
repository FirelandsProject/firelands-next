#include <gtest/gtest.h>
#include <shared/network/ByteBuffer.h>

using namespace Firelands;

TEST(ByteBufferTest, WriteAndReadBasicTypes) {
    ByteBuffer buffer;
    buffer.Append<uint32>(0x12345678);
    buffer.Append<uint8>(0xAB);
    buffer.Append<uint16>(0xCDEF);

    EXPECT_EQ(buffer.Read<uint32>(), 0x12345678);
    EXPECT_EQ(buffer.Read<uint8>(), 0xAB);
    EXPECT_EQ(buffer.Read<uint16>(), 0xCDEF);
}

TEST(ByteBufferTest, WriteAndReadString) {
    ByteBuffer buffer;
    std::string testStr = "Firelands";
    buffer.Append(testStr);
    
    EXPECT_EQ(buffer.ReadString(), "Firelands");
}

TEST(ByteBufferTest, ReadEmptyBuffer) {
    ByteBuffer buffer;
    EXPECT_EQ(buffer.Read<uint32>(), 0);
}

TEST(ByteBufferTest, ResizeAndAccess) {
    ByteBuffer buffer;
    buffer.Resize(10);
    buffer[5] = 0xAA;
    
    EXPECT_EQ(buffer.Data()[5], 0xAA);
    EXPECT_EQ(buffer.Size(), 10);
}
