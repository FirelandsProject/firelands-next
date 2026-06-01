# Firelands - WoW Cataclysm Emulator (4.3.4.15595)

## Build Commands

**Agent note:** NEVER compile the project automatically. After any change, ASK the user for confirmation before compiling. Only compile if the user explicitly authorizes it.

## Sub-agent delegation

Before implementing anything, **analyze complexity** and decide whether work stays on the main agent or should be delegated to a sub-agent. When in doubt, prefer delegation for work that would bloat main context or spans domains.

### When to delegate

| Signal | Prefer |
|--------|--------|
| Single file, obvious change, under ~30 min scope | Main agent (inline) |
| Broad codebase search / “where is X?” | `explore` or `cavecrew-investigator` (readonly) |
| Surgical 1–2 file edit with clear spec | `cavecrew-builder` |
| PR/diff review, audit | `cavecrew-reviewer` |
| Shell/git/CI only | `shell` |
| Multiple domains (auth + world + DB + core) | Parallel sub-agents per [Delegation](.agent/skills/Delegation/SKILL.md) roles |
| Large feature, protocol correctness, security, cross-cutting refactor | `generalPurpose` (or domain role) with a **strong** model |

**Must delegate** when a task spans multiple domains (Auth, World, DB, Core) — see [Delegation skill](.agent/skills/Delegation/SKILL.md).

### Model selection (quality first)

Pick the sub-agent **type** and **model** for the hardest part of the task, not the easiest step.

- **Default:** Omit `model` on the Task tool so the sub-agent uses the same model as the parent (no quality drop).
- **User-requested model:** If the user names a model, use only allowed Task `model` slugs; if unavailable, say so and pick the closest tier.
- **Prefer stronger / reasoning models** for: new opcodes/packets, crypto/auth, hexagonal refactors, concurrency, ambiguous requirements, multi-file design.
- **Lighter models are OK** for: readonly exploration, mechanical renames, formatting-only edits, running scripted commands — only when the spec is unambiguous and mistakes are cheap to fix.
- **Never** trade quality on correctness-critical paths (SRP6, DB migrations, packet layouts, TDD business rules) to save tokens or latency.

Launch **parallel** sub-agents when domains or search areas are independent; synthesize results on the main agent before committing to a design.

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
- Ref world text: `python3 tools/sql/import_ref_npc_text.py` → `sql/migrations/34_world_npc_text_data.sql`.
- Ref gossip menus: `python3 tools/sql/import_ref_gossip.py` → `sql/migrations/35_world_gossip_data.sql`.
- Ref quest gossip (starters): `python3 tools/sql/import_ref_quest_gossip.py` → `sql/migrations/38_world_quest_gossip_data.sql`.
- Quest chain prerequisites: `python3 tools/sql/import_ref_quest_prev_quest.py` → `sql/migrations/68_world_quest_prev_quest_data.sql` (DDL in `67_world_quest_template_prev_quest.sql`).
- Phase catalogs (zone phasing + phase groups): `python3 tools/sql/import_ref_phase_data.py` → `sql/migrations/55_world_phase_catalog_data.sql` (needs `data/dbc/PhaseXPhaseGroup.dbc`).

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

## Cursor slash commands

Type `/` in chat to run project commands from `.cursor/commands/` (tracked in git).

| Command | Definition |
|---------|------------|
| `/auto-commit` | [.cursor/commands/auto-commit.md](.cursor/commands/auto-commit.md) — analyze diff, apply Firelands version bumps (`merge_migrations`, `db_version`), split into Conventional Commits; **no push** unless requested |

A global copy may exist at `~/.cursor/commands/auto-commit.md`; prefer the **project** file when working in this repo. See [.cursor/README.md](.cursor/README.md).

## Cursor Rules (Auto-loaded)

Project rules live in `.cursor/rules/` and load automatically in Cursor:

| Rule | Scope |
|------|-------|
| `firelands-core.mdc` | Always — mandates, build policy, delegation |
| `firelands-architecture.mdc` | Always — hexagonal architecture |
| `firelands-tdd.mdc` | Always — TDD workflow |
| `firelands-cpp.mdc` | C++ sources under `src/`, `tests/` |
| `firelands-cmake.mdc` | `CMakeLists.txt`, `cmake/` |
| `firelands-sql.mdc` | `sql/` |
| `firelands-auth.mdc` | `src/auth/` |
| `firelands-world.mdc` | `src/world/`, `src/shared/network/` |

Deep workflow detail remains in `.agent/skills/` (read on demand).

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
| Delegation | `.agent/skills/Delegation/SKILL.md` | Multi-domain tasks; sub-agent planning (see **Sub-agent delegation** above) |

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
| spdlog | 1.14.1 | Logging (`SPDLOG_USE_STD_FORMAT` with C++20) |
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
- Reference implementation (local clone, not vendored): `firelands-cata-ref/`.

### Reference scope (Cataclysm 4.3.4 only)

This emulator targets **World of Warcraft Cataclysm 4.3.4** (build **15595**). When verifying server logic, SQL, packets, or gameplay behavior:

- **Use** Cataclysm 4.3.4 sources (e.g. local clone `firelands-cata-ref/`, and other refs that match this build).
- **Do not** use **WotLK** or **3.3.5a** server code, SQL, opcodes, or patterns as reference—they follow different standards and will not align with this project.
- If a snippet or doc is from another expansion, discard it or find the Cataclysm 4.3.4 equivalent before implementing.

## Logs

- **World server**: `logs/firelands-world.log`
- **Auth server**: `logs/firelands-auth.log`

## Cross-Platform Notes

- **Windows**: MSVC or MinGW
- **Linux**: GCC or Clang
- **macOS**: Clang (Apple Clang)
- Use `std::filesystem` for paths
- Use `std::thread` instead of platform-specific threading