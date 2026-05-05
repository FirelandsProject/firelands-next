# Firelands - WoW Cataclysm Emulator (4.3.4.15595)

## Build Commands

**Agent note:** NEVER compile the project automatically. After any change, ASK the user for confirmation before compiling. Only compile if the user explicitly authorizes it.
**Agent note:** For complex tasks spanning multiple domains, you MUST use sub agents (Auth, World, DB, Core) as defined in the Delegation skill.

### Configuration
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```
- **Generator**: MUST use Ninja (not Make)
- **ccache**: Auto-detected and enabled if installed

**Note**: All build outputs are placed in the `build/` directory as specified above.

### Building
```bash
ninja -C build                    # Full build
ninja -C build <target>           # Specific target (e.g., auth, world, FirelandsDevTools)
ninja -C build auth world         # Build auth and world servers
```

### Running Tests
```bash
cmake -B build -G Ninja -DFIRELANDS_BUILD_TESTS=ON  # Enable tests
ninja -C build
ctest --test-dir build -R <pattern>                 # Run tests matching pattern
ctest --test-dir build                             # Run all tests
```

### Other Targets
```bash
cmake --build build --target merge-migrations     # Merge SQL migrations
```

## Architecture (Hexagonal / Ports & Adapters)

```
src/
├── shared/           # Common utilities, Logger, Common.h
├── domain/           # Entities, Value Objects, Repository Interfaces (Ports)
├── application/     # Use cases, Application Services
├── infrastructure/  # MySQL adapters, REST adapters, External wrappers (Adapters)
├── auth/            # Auth server executable
├── world/           # World server executable
└── tools/           # DevTools executable
```

### Dependency Rule
- `domain/` must NOT import from `application/` or `infrastructure/`
- All external dependencies flow inward: infrastructure -> application -> domain
- Communication via abstract interfaces (ports)

## Executables

| Target | Binary | Purpose |
|--------|--------|---------|
| `auth` | `build/bin/auth` | Authentication server (login, session) |
| `world` | `build/bin/world` | Game server (realms, characters, gameplay) |
| `FirelandsDevTools` | `build/bin/FirelandsDevTools` | Development utilities |

## Database

### Local Development
```bash
docker-compose up -d db
```
- **Image**: mysql:8.0
- **Port**: 3306
- **Credentials**: root/root, firelands/firelands
- **Databases**: auth, characters, world

### Migrations / bundled schema
- Runtime migrator: `sql/init/*.sql` plus optional `sql/migrations/*.sql` (see `DatabaseMigrator`).
- Docker first boot: `docker/mysql-init/docker_grants_firelands_user.sql` then **`sql/bundled/firelands_auth.sql`**, **`firelands_characters.sql`**, **`firelands_world.sql`**, **`zz_seed_schema_migrations.sql`**.
- Regenerate bundles when split migrations exist: `python3 tools/merge_migrations.py` or `cmake --build build --target merge-migrations`. Optional scratch: `sql/merged/`.

## Testing

### Unit Tests
- Location: `tests/unit/`
- Framework: GoogleTest (gtest/gmock)
- Framework: GMock for mocking ports

### TDD Workflow
1. **Red**: Write failing test first
2. **Green**: Write minimal code to pass
3. **Refactor**: Clean up while keeping tests green

### Integration Tests
- Adapters tested against real database
- Use docker-compose MySQL for integration tests

## Skills (Agent Constraints)

### Location
- **DO NOT** use `.skills/` - skills are in `.agent/skills/`

### Active Skills
| Skill | File | When to Use |
|-------|------|-------------|
| Mandates | `.agent/skills/Mandates/SKILL.md` | Always (baseline) |
| Architecture | `.agent/skills/Architecture/SKILL.md` | New features, services, repositories |
| TDD | `.agent/skills/TDD/SKILL.md` | Business logic changes |
| TechStack | `.agent/skills/TechStack/SKILL.md` | Platform/cross-platform work |
| Optimization | `.agent/skills/Optimization/SKILL.md` | Build/target changes |
| Language | `.agent/skills/Language/SKILL.md` | Code/comments/naming |
| Delegation | `.agent/skills/Delegation/SKILL.md` | Large multi-domain tasks |

## Language & Communication

- **User Communication**: English (caveman style)
- **Code**: English only (variables, functions, types, comments)
- **Git**: English only (commit messages, branch names)

## Build Optimization

### Precompiled Headers (PCH)
- Defined in `CMakeLists.txt`: `${PROJECT_PCH_HEADERS}`, `${TEST_PCH_HEADERS}`
- Heavy headers precompiled: STL, spdlog, nlohmann/json, shared/Common.h, shared/Logger.h

### Adding New Targets
Must include PCH:
```cmake
target_precompile_headers(<target_name> PRIVATE ${PROJECT_PCH_HEADERS})
```

### PCH Requirement
- spdlog MUST be included via `<shared/Logger.h>` for SPDLOG_LEVEL_NAMES to apply

## Dependencies (Fetched via CMake)

| Library | Version | Purpose |
|---------|---------|---------|
| Boost | (system) | Threading |
| GoogleTest | 1.14.0 | Testing |
| spdlog | 1.14.1 | Logging |
| MariaDB Connector/C | 3.3.8 | MySQL client |
| MariaDB Connector/C++ | 1.1.7 | MySQL C++ |
| nlohmann/json | 3.11.2 | JSON |
| yaml-cpp | 0.8.0 | YAML config |
| Lua | 5.4.7 | Gameplay scripting |
| FTXUI | 5.0.0 | Console UI |
| StormLib | 9.26 | MPQ archive handling |

## Configuration Files

| File | Purpose |
|------|---------|
| `authserver.yaml` | Auth server config |
| `worldserver.yaml` | World server config |
| `cmake` | Build configuration |
| `compile_commands.json` | IDE integration (auto-generated) |

## Key References

- Documentation: `docs/EN/README.md`
- Module guides: `docs/EN/modules/README.md`
- Reference implementation (local clone, not vendored): `firelands-cata-ref/` or `firelands-core-ref/` (local clone names)

## Logs

- **World server**: `logs/firelands-world.log`
- **Auth server**: `logs/firelands-auth.log`

## Cross-Platform Notes

- **Windows**: MSVC or MinGW
- **Linux**: GCC or Clang
- **macOS**: Clang (Apple Clang)
- Use `std::filesystem` for paths
- Use `std::thread` instead of platform-specific threading