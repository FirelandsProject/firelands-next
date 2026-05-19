#include <gtest/gtest.h>
#include <array>
#include <shared/network/ByteBuffer.h>
#include <span>

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

TEST(ByteBufferTest, AppendAndReadSpan) {
  ByteBuffer buffer;
  const std::array<uint8, 4> bytes = {0x01, 0x02, 0x03, 0x04};
  buffer.Append(std::span<const uint8>(bytes));

  EXPECT_EQ(buffer.Size(), 4u);
  EXPECT_EQ(buffer.AsSpan().size(), 4u);
  EXPECT_EQ(buffer.AsSpan()[0], 0x01);

  std::array<uint8, 2> out{};
  EXPECT_EQ(buffer.Read(std::span<uint8>(out)), 2u);
  EXPECT_EQ(out[0], 0x01);
  EXPECT_EQ(out[1], 0x02);
  EXPECT_EQ(buffer.UnreadSpan().size(), 2u);
}

TEST(ByteBufferTest, ResizeAndAccess) {
    ByteBuffer buffer;
    buffer.Resize(10);
    buffer[5] = 0xAA;
    
    EXPECT_EQ(buffer.Data()[5], 0xAA);
    EXPECT_EQ(buffer.Size(), 10);
}

TEST(ByteBufferTest, AppendPackedTime) {
    ByteBuffer buffer;
    // 2026-04-27 18:10:00 (Mon)
    // tm_year = 126 (2026 - 1900)
    // tm_mon = 3 (April, 0-indexed)
    // tm_mday = 27
    // tm_wday = 1 (Monday)
    // tm_hour = 18
    // tm_min = 10
    
    std::tm t = {};
    t.tm_year = 126;
    t.tm_mon = 3;
    t.tm_mday = 27;
    t.tm_wday = 1;
    t.tm_hour = 18;
    t.tm_min = 10;
    
    time_t timeVal = mktime(&t);
    buffer.AppendPackedTime(timeVal);
    
    uint32 packed = buffer.Read<uint32>();
    
    // Expected: (126-100)<<24 | 3<<20 | (27-1)<<14 | 1<<11 | 18<<6 | 10
    // 26 << 24 = 0x1A000000
    // 3 << 20  = 0x00300000
    // 26 << 14 = 0x00068000
    // 1 << 11  = 0x00000800
    // 18 << 6  = 0x00000480
    // 10       = 0x0000000A
    // Sum      = 0x1A368C8A
    
    EXPECT_EQ(packed, 0x1A368C8A);
}
