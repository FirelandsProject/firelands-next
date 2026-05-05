#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <domain/world/Aura.h>
#include <domain/world/Player.h>

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

// Player aura management tests
TEST(PlayerAuraTests, PlayerUpdatesAurasCorrectly) {
    // Create a player
    std::unique_ptr<Player> player = std::make_unique<Player>(0x1000ULL, nullptr);
    
    // Create an expired aura (already expired)
    auto expiredTime = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    Aura expiredAura(123, 4, 10, 6, 0x2000ULL, expiredTime);
    
    // Create a valid aura
    auto validTime = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    Aura validAura(456, 5, 15, 8, 0x3000ULL, validTime);
    
    // Add both auras
    player->AddAura(expiredAura);
    player->AddAura(validAura);
    
    // Verify both auras were added
    EXPECT_TRUE(player->HasAura(123));
    EXPECT_TRUE(player->HasAura(456));
    
    // Update auras (should remove expired ones)
    player->UpdateAuras();
    
    // Verify expired aura was removed
    EXPECT_FALSE(player->HasAura(123));
    
    // Verify valid aura remains
    EXPECT_TRUE(player->HasAura(456));
    
    // Verify only one active aura remains
    auto activeAuras = player->GetActiveAuras();
    EXPECT_EQ(activeAuras.size(), 1u);
    EXPECT_EQ(activeAuras[0].GetSpellId(), 456u);
}

TEST(PlayerAuraTests, PlayerCanRemoveAura) {
    // Create a player
    auto player = std::make_unique<Player>(0x1000ULL, nullptr);
    
    // Create and add an aura
    Aura aura(123, 4, 10, 6, 0x2000ULL, 
              std::chrono::steady_clock::now() + std::chrono::seconds(30));
    player->AddAura(aura);
    
    // Verify aura exists
    EXPECT_TRUE(player->HasAura(123));
    
    // Remove aura
    player->RemoveAura(123);
    
    // Verify aura is gone
    EXPECT_FALSE(player->HasAura(123));
    
    // Verify no active auras
    auto activeAuras = player->GetActiveAuras();
    EXPECT_EQ(activeAuras.size(), 0u);
}

TEST(PlayerAuraTests, PlayerRemovesExpiredAurasOnUpdate) {
    // Create a player
    auto player = std::make_unique<Player>(0x1000ULL, nullptr);
    
    // Create an expired aura (already expired)
    auto expiredTime = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    Aura expiredAura(123, 4, 10, 6, 0x2000ULL, expiredTime);
    
    // Create a valid aura
    auto validTime = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    Aura validAura(456, 5, 15, 8, 0x3000ULL, validTime);
    
    // Add both auras
    player->AddAura(expiredAura);
    player->AddAura(validAura);
    
    // Verify both auras were added
    EXPECT_TRUE(player->HasAura(123));
    EXPECT_TRUE(player->HasAura(456));
    
    // Update auras (should remove expired ones)
    player->UpdateAuras();
    
    // Verify expired aura was removed
    EXPECT_FALSE(player->HasAura(123));
    
    // Verify valid aura remains
    EXPECT_TRUE(player->HasAura(456));
    
    // Verify only one active aura remains
    auto activeAuras = player->GetActiveAuras();
    EXPECT_EQ(activeAuras.size(), 1u);
    EXPECT_EQ(activeAuras[0].GetSpellId(), 456u);
}