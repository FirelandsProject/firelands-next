#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <domain/world/Aura.h>

using namespace Firelands;

TEST(AuraTests, ConstructorInitializesFieldsCorrectly) {
    std::uint32_t spellId = 123;
    std::uint32_t auraEffectType = 4;
    std::int32_t basePoints = 10;
    std::int32_t dieSides = 6;
    std::uint64_t casterGuid = 0x1000ULL;
    
    auto now = std::chrono::steady_clock::now();
    auto expireTime = now + std::chrono::seconds(30);
    
    Aura aura(spellId, auraEffectType, basePoints, dieSides, casterGuid, expireTime);
    
    EXPECT_EQ(aura.GetSpellId(), spellId);
    EXPECT_EQ(aura.GetAuraEffectType(), auraEffectType);
    EXPECT_EQ(aura.GetBasePoints(), basePoints);
    EXPECT_EQ(aura.GetDieSides(), dieSides);
    EXPECT_EQ(aura.GetCasterGuid(), casterGuid);
    EXPECT_EQ(aura.GetExpireTime(), expireTime);
}

TEST(AuraTests, IsExpiredReturnsFalseWhenNotExpired) {
    std::uint32_t spellId = 123;
    std::uint32_t auraEffectType = 4;
    std::int32_t basePoints = 10;
    std::int32_t dieSides = 6;
    std::uint64_t casterGuid = 0x1000ULL;
    
    auto now = std::chrono::steady_clock::now();
    auto expireTime = now + std::chrono::seconds(30); // Future time
    
    Aura aura(spellId, auraEffectType, basePoints, dieSides, casterGuid, expireTime);
    
    EXPECT_FALSE(aura.IsExpired());
}

TEST(AuraTests, IsExpiredReturnsTrueWhenExpired) {
    std::uint32_t spellId = 123;
    std::uint32_t auraEffectType = 4;
    std::int32_t basePoints = 10;
    std::int32_t dieSides = 6;
    std::uint64_t casterGuid = 0x1000ULL;
    
    auto now = std::chrono::steady_clock::now();
    auto expireTime = now - std::chrono::seconds(30); // Past time
    
    Aura aura(spellId, auraEffectType, basePoints, dieSides, casterGuid, expireTime);
    
    EXPECT_TRUE(aura.IsExpired());
}

TEST(AuraTests, GetMagnitudeReturnsBasePointsWhenDieSidesZero) {
    std::uint32_t spellId = 123;
    std::uint32_t auraEffectType = 4;
    std::int32_t basePoints = 15;
    std::int32_t dieSides = 0; // No random component
    std::uint64_t casterGuid = 0x1000ULL;
    
    auto now = std::chrono::steady_clock::now();
    auto expireTime = now + std::chrono::seconds(30);
    
    Aura aura(spellId, auraEffectType, basePoints, dieSides, casterGuid, expireTime);
    
    // When dieSides is 0, magnitude should equal basePoints
    EXPECT_EQ(aura.GetMagnitude(), basePoints);
}

TEST(AuraTests, GetMagnitudeIncludesRandomComponentWhenDieSidesPositive) {
    std::uint32_t spellId = 123;
    std::uint32_t auraEffectType = 4;
    std::int32_t basePoints = 10;
    std::int32_t dieSides = 6; // Random component 0-5
    std::uint64_t casterGuid = 0x1000ULL;
    
    auto now = std::chrono::steady_clock::now();
    auto expireTime = now + std::chrono::seconds(30);
    
    Aura aura(spellId, auraEffectType, basePoints, dieSides, casterGuid, expireTime);
    
    std::int32_t magnitude = aura.GetMagnitude();
    // Magnitude should be between basePoints and basePoints + dieSides
    EXPECT_GE(magnitude, basePoints);
    EXPECT_LE(magnitude, basePoints + dieSides);
}

TEST(AuraTests, GetMagnitudeHandlesNegativeDieSides) {
    std::uint32_t spellId = 123;
    std::uint32_t auraEffectType = 4;
    std::int32_t basePoints = 10;
    std::int32_t dieSides = -2; // Negative dieSides
    std::uint64_t casterGuid = 0x1000ULL;
    
    auto now = std::chrono::steady_clock::now();
    auto expireTime = now + std::chrono::seconds(30);
    
    Aura aura(spellId, auraEffectType, basePoints, dieSides, casterGuid, expireTime);
    
    // When dieSides is negative, magnitude should equal basePoints (negative dieSides treated as 0)
    EXPECT_EQ(aura.GetMagnitude(), basePoints);
}