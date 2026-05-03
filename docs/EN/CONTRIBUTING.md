# Contributing Guide

This guide outlines how to contribute to Firelands, including development workflow, coding standards, and best practices.

## Prerequisites

Before contributing, make sure you understand:

1. **Hexagonal Architecture** - See [Architecture Overview](modules/README.md)
2. **TDD Workflow** - Write tests first, then implementation
3. **Build System** - CMake + Ninja (see [Developer Setup](DEVELOPER_SETUP.md))
4. **Language** - All code, comments, and git in English

## Development Workflow

### 1. Finding Work

- Check the [Roadmap](ROADMAP.md) for planned features
- Look at GitHub issues labeled `good first issue`
- Review existing documentation gaps

### 2. Starting Work

```bash
# Create a new branch for your feature
git checkout -b feature/my-new-feature
# Or for bug fixes
git checkout -b fix/description-of-bug
```

Branch naming conventions:
- `feature/` - New features
- `fix/` - Bug fixes
- `refactor/` - Code refactoring
- `docs/` - Documentation only

### 3. Development Process

1. **Write tests first** (TDD) - See [Testing Guide](TESTING.md)
2. **Implement the feature**
3. **Verify tests pass**
4. **Run linter/formatter** (if configured)
5. **Commit with clear messages**

### 4. Commit Messages

Follow the pattern:
```
<type>(<scope>): <description>

[optional body]
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`

Examples:
```
feat(auth): add SRP6a password reset flow
fix(world): correct spell damage calculation for area spells
docs(database): document new migration schema
refactor(spell): extract damage formula to domain layer
```

### 5. Submitting Changes

1. Push your branch:
   ```bash
   git push origin feature/my-new-feature
   ```

2. Create a Pull Request on GitHub
3. Fill out the PR template
4. Wait for code review

## Code Standards

### Language

- **Code**: English only (variables, functions, classes)
- **Comments**: English only
- **Git messages**: English only
- **Documentation**: English for EN/, Spanish for ES/

### C++ Style

- **C++17** standard required
- Use `snake_case` for variables and functions
- Use `PascalCase` for types and classes
- Use `UPPER_SNAKE_CASE` for constants and enums
- Use `kebab-case` for file names

Example:
```cpp
class MyClass {
public:
    void DoSomething();
    int CalculateValue() const;

private:
    int internal_state_ = 0;
    static constexpr int kMaxValue = 100;
};

void MyClass::DoSomething() {
    for (auto& item : items_) {
        item.Process();
    }
}
```

### Headers

- Use include guards or `#pragma once`
- Order includes: 1) associated header, 2) project, 3) system
- Use forward declarations when possible

```cpp
// associated.h
#ifndef PROJECT_MODULE_COMPONENT_H
#define PROJECT_MODULE_COMPONENT_H

#include <module/other_component.h>  // project
#include <vector>                     // system
#include <memory>                     // system

// Forward declarations
class IRepository;

class Component {
    // ...
};
#endif
```

### Architecture Rules

**Dependency Direction** (hexagonal):
```
Infrastructure → Application → Domain → Shared
```

- `domain/` MUST NOT import from `application/` or `infrastructure/`
- Use **ports** (interfaces) for dependency inversion
- Implement **adapters** in `infrastructure/`

Example:
```cpp
// domain/repositories/ICharacterRepository.h
#pragma once

#include <domain/models/Character.h>
#include <optional>

class ICharacterRepository {
public:
    virtual ~ICharacterRepository() = default;
    virtual std::optional<Character> FindByGuid(uint32 guid) = 0;
    virtual void Save(const Character& character) = 0;
};

// infrastructure/persistence/MySqlCharacterRepository.h
#pragma once

#include <domain/repositories/ICharacterRepository.h>
#include <mysql_connection.h>  // infrastructure only

class MySqlCharacterRepository : public ICharacterRepository {
    // implementation...
};
```

### Error Handling

- Use exceptions for exceptional situations
- Return `std::optional<T>` for operations that may fail
- Use result types for operations with multiple outcomes

### Logging

- Use `Logger` from `shared/Logger.h`
- Include context in log messages
- Use appropriate levels: `trace`, `debug`, `info`, `warn`, `error`

```cpp
#include <shared/Logger.h>

LOG_INFO("Player {} logged in from {}", player.GetName(), ip_address);
LOG_ERROR("Database connection failed: {}", error.what());
```

## SQL Standards

### Migration Files

- Place in `sql/migrations/`
- Prefix with ordering number: `001_`, `002_`, etc.
- Use idempotent operations
- Include comments explaining the change

```sql
-- Add new column for future feature
-- See issue #123
ALTER TABLE account
    ADD COLUMN IF NOT EXISTS `some_feature_flag` TINYINT DEFAULT 0;

-- Add index for better query performance
CREATE INDEX IF NOT EXISTS idx_character_account
    ON characters(account);
```

### Database Code

- All SQL in infrastructure layer
- Use parameterized queries (never string concatenation)
- Follow naming conventions from existing schemas

## LUA Standards

- Scripts go in `scripts/lua/`
- Use descriptive function names
- Follow existing script patterns (see [LUA Scripting](LUA_SCRIPTING.md))

## Testing Standards

### Unit Tests

- Use GoogleTest framework
- Test one thing per test case
- Use descriptive test names
- Follow AAA pattern (Arrange, Act, Assert)

```cpp
TEST(CharacterService, CreateCharacter_SetsDefaultValues) {
    // Arrange
    auto service = CreateTestService();

    // Act
    auto character = service.CreateCharacter(account_id, "TestChar", RACE_HUMAN, CLASS_WARRIOR);

    // Assert
    EXPECT_EQ(character.GetName(), "TestChar");
    EXPECT_EQ(character.GetLevel(), 1);
    EXPECT_EQ(character.GetMoney(), 0);
}
```

### Integration Tests

- Use real database when possible
- Mock external services (world server, REST)
- Clean up test data after tests

## Documentation Standards

### Adding Documentation

1. Create or update file in `docs/EN/` (English) and `docs/ES/` (Spanish)
2. Use clear, descriptive headers
3. Include code examples where helpful
4. Keep documentation in sync with code

### Documentation Files

- **Module docs**: `docs/EN/modules/*.md`
- **Guides**: `docs/EN/*.md` (Developer Setup, Database Schema, etc.)
- **API docs**: Inline in headers

## Common Contribution Types

### Adding a New Feature

1. Create feature branch
2. Add domain model (if needed)
3. Add repository interface
4. Add service layer
5. Add infrastructure implementation
6. Add tests
7. Document the feature
8. Submit PR

### Fixing a Bug

1. Create fix branch
2. Write failing test first
3. Fix the bug
4. Verify test passes
5. Submit PR

### Adding a Database Migration

1. Create migration file in `sql/migrations/`
2. Write idempotent SQL
3. Test on local database
4. Update bundled SQL (`merge-migrations` target)
5. Document the change in schema guide

### Adding a GM Command

1. Add permission to `Permissions.h`
2. Register command in `CommandService.cpp`
3. Implement handler method
4. Add tests for the command
5. Document in [GM Administration](modules/gm-administration.md)

## Code Review Guidelines

### For Reviewers

- Be constructive and respectful
- Focus on logic, architecture, and style
- Suggest improvements, don't demand
- Approve when ready

### For Authors

- Respond to all comments
- Don't take criticism personally
- Explain your decisions
- Keep PRs focused and reasonable size

## Getting Help

- **Documentation**: Check `docs/` folder
- **Issues**: Open on GitHub
- **Discussion**: Use GitHub Discussions
- **Reference**: See `firelands-cata-ref/` for implementation examples

## Recognition

Contributors are acknowledged in the project. Thank you for your work!