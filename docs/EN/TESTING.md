# Testing Guide

This guide explains how to write, run, and maintain tests in Firelands.

## Overview

Firelands uses **GoogleTest** (gtest/gmock) for unit testing. The project follows a strict **TDD (Test-Driven Development)** workflow.

## Test Framework

- **GoogleTest** - Unit testing framework
- **GoogleMock** - Mocking framework for interfaces

Both are fetched via CMake FetchContent (see `CMakeLists.txt`).

## Test Location

```
tests/
└── unit/
    ├── shared/           # Tests for shared library
    ├── domain/          # Tests for domain layer
    ├── application/     # Tests for application layer
    ├── infrastructure/ # Tests for infrastructure layer
    ├── tools/           # Tests for DevTools
    └── vmap/            # Tests for vmap extractors
```

## Running Tests

### Build with Tests Enabled

```bash
# Configure with tests
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFIRELANDS_BUILD_TESTS=ON

# Build
ninja -C build
```

### Run All Tests

```bash
ctest --test-dir build
```

### Run Specific Test Suites

```bash
# Run domain tests
ctest --test-dir build -R domain

# Run application tests
ctest --test-dir build -R application

# Run infrastructure tests
ctest --test-dir build -R infrastructure

# Run shared tests
ctest --test-dir build -R shared
```

### Run Single Test

```bash
# Using gtest filter
ctest --test-dir build -R "CharacterServiceTests.CreateCharacter"
```

## TDD Workflow

### Red-Green-Refactor

1. **Red** - Write a failing test first
2. **Green** - Write minimal code to make it pass
3. **Refactor** - Clean up while keeping tests green

### Example: Adding a New Feature

```cpp
// Step 1: Red - Write failing test
// tests/unit/application/CharacterServiceTests.cpp

#include <gtest/gtest.h>
#include <application/services/CharacterService.h>

TEST(CharacterService, CreateCharacter_SetsDefaultMoney) {
    // Arrange
    auto repository = std::make_unique<MockCharacterRepository>();
    auto service = CharacterService(std::move(repository));

    // Act
    auto character = service.CreateCharacter(
        1, "TestPlayer", RACE_HUMAN, CLASS_WARRIOR
    );

    // Assert - this will FAIL initially
    EXPECT_EQ(character.GetMoney(), 0);
}

// Step 2: Green - Implement minimal code to pass

// In CharacterService.cpp
Character CharacterService::CreateCharacter(...) {
    Character character;
    character.SetMoney(0);  // Add this line
    return character;
}

// Step 3: Refactor - Clean up, add more tests
```

## Writing Tests

### Test File Structure

```cpp
// tests/unit/domain/MyEntityTests.cpp

#include <gtest/gtest.h>
#include <domain/models/MyEntity.h>

class MyEntityTests : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }

    void TearDown() override {
        // Common cleanup
    }
};

// Test cases
TEST_F(MyEntityTests, MethodName_ExpectedBehavior) {
    // Arrange
    MyEntity entity;

    // Act
    auto result = entity.DoSomething();

    // Assert
    EXPECT_EQ(result, expected_value);
}
```

### Assertions

Common assertions:

| Assertion | Description |
|-----------|-------------|
| `EXPECT_EQ(a, b)` | a == b |
| `EXPECT_NE(a, b)` | a != b |
| `EXPECT_TRUE(a)` | a is true |
| `EXPECT_FALSE(a)` | a is false |
| `EXPECT_FLOAT_EQ(a, b)` | a approx equals b |
| `EXPECT_DOUBLE_EQ(a, b)` | double precision |
| `EXPECT_STREQ(a, b)` | string equality |
| `EXPECT_THROW(code, type)` | code throws exception |
| `EXPECT_DEATH(code, regex)` | code crashes (death test) |

### Test Fixtures

Use fixtures for common setup/teardown:

```cpp
class CharacterServiceTests : public ::testing::Test {
protected:
    std::unique_ptr<ICharacterRepository> mockRepo_;
    std::unique_ptr<CharacterService> service_;

    void SetUp() override {
        mockRepo_ = std::make_unique<MockCharacterRepository>();
        service_ = std::make_unique<CharacterService>(mockRepo_.get());
    }
};
```

## Mocking

### Using GoogleMock

```cpp
// Mock repository interface
#include <gmock/gmock.h>
#include <domain/repositories/ICharacterRepository.h>

class MockCharacterRepository : public ICharacterRepository {
public:
    MOCK_METHOD(std::optional<Character>, FindByGuid, (uint32 guid), (override));
    MOCK_METHOD(void, Save, (const Character&), (override));
    MOCK_METHOD(void, Delete, (uint32 guid), (override));
    MOCK_METHOD(std::vector<Character>, FindByAccount, (int accountId), (override));
};
```

### Setting Expectations

```cpp
TEST_F(CharacterServiceTests, GetCharacter_CallsRepository) {
    // Arrange
    EXPECT_CALL(*mockRepo_, FindByGuid(1))
        .WillOnce(Return(Character{...}));

    // Act
    auto result = service_->GetCharacter(1);

    // Assert
    EXPECT_TRUE(result.has_value());
}
```

### Returning Values

```cpp
// Return a value
EXPECT_CALL(*mockRepo_, FindByGuid)
    .WillOnce(Return(character));

// Return different values for multiple calls
EXPECT_CALL(*mockRepo_, FindByGuid)
    .WillOnce(Return(char1))
    .WillOnce(Return(char2));

// Use a lambda
EXPECT_CALL(*mockRepo_, Save)
    .WillOnce([](const Character& c) {
        // Do something
    });

// Throw exception
EXPECT_CALL(*mockRepo_, FindByGuid)
    .WillOnce(Throw(std::runtime_error("not found")));
```

## Best Practices

### Test Naming

Use descriptive names: `ClassName_Method_ExpectedBehavior`

```cpp
TEST(CharacterService, CreateCharacter_SetsDefaultLevel)
TEST(Player, TakeDamage_AppliesDamageAndReturnsHealth)
TEST(Spell, CalculateDamage_AccountsForSpellPower)
```

### AAA Pattern

1. **Arrange** - Set up test data and objects
2. **Act** - Execute the method being tested
3. **Assert** - Verify the expected outcome

### One Assertion Per Test

Focus on one behavior per test for clarity:

```cpp
// Good - one behavior per test
TEST(Character, SetName_ChangesName) { }
TEST(Character, SetName_ValidatesLength) { }

// Bad - multiple behaviors
TEST(Character, SetName_ChangesNameAndValidatesLengthAndNotifies) { }
```

### Keep Tests Independent

- Each test should run independently
- Don't rely on execution order
- Clean up after yourself

### Test Edge Cases

- Empty input
- Null values
- Boundary values
- Maximum values
- Invalid input

## Code Coverage

### Building with Coverage

```bash
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DFIRELANDS_BUILD_TESTS=ON \
    -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage"

ninja -C build

# Run tests
ctest --test-dir build

# Generate coverage report (requires lcov)
lcov --capture --directory build --output-file coverage.info
lcov --remove coverage.info '*tests*' --output-file filtered.info
genhtml filtered.info --output-directory coverage_report
```

## Integration Tests

### Against Real Database

```cpp
class MySqlRepositoryTests : public ::testing::Test {
protected:
    std::unique_ptr<MySqlConnection> conn_;

    void SetUp() override {
        conn_ = std::make_unique<MySqlConnection>("...");
        // Run migrations if needed
    }
};

TEST_F(MySqlRepositoryTests, Save_InsertsRecord) {
    // Use real database connection
    MySqlCharacterRepository repo(conn_.get());

    Character c;
    c.SetName("Test");
    repo.Save(c);

    auto loaded = repo.FindByName("Test");
    EXPECT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->GetName(), "Test");
}
```

### Docker Integration

Use Docker for integration tests:
```bash
# Start database
docker-compose up -d db

# Run integration tests
ctest --test-dir build -R integration

# Clean up
docker-compose down
```

## CI/CD

Tests run automatically on every PR. Add tests to your PR to ensure code quality.

## Common Test Patterns

### Testing Protected/Private Methods

Use FRIEND_TEST or expose via protected:

```cpp
class MyClass {
    FRIEND_TEST(MyClassTests, InternalMethod);
protected:
    void InternalMethod();  // Testable via friend
};
```

### Testing Exceptions

```cpp
TEST(Player, EquipItem_ThrowsOnInvalidSlot) {
    Player player;
    EXPECT_THROW(player.EquipItem(999, item), std::invalid_argument);
}
```

### Testing Date/Time

Be careful with time-dependent tests. Use dependency injection or mock time.

## Troubleshooting

### Tests Won't Compile

- Check include paths
- Ensure PCH includes gtest/gmock

### Tests Run Slowly

- Check for I/O in tests
- Use mocks for external dependencies

### Flaky Tests

- Remove timing dependencies
- Use deterministic mocks

## See Also

- [Developer Setup](DEVELOPER_SETUP.md)
- [Contributing Guide](CONTRIBUTING.md)
- [Application Layer](modules/application.md)
- [GoogleTest Documentation](https://google.github.io/googletest/)