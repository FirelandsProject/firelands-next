# Tools, SQL migrations, tests, and build

## CMake / build (`CMakeLists.txt`)

- **C++20** project **Firelands** with FetchContent for googletest, spdlog, MariaDB C/C++, nlohmann_json, yaml-cpp, Lua, StormLib.
- **Libraries:** `FirelandsShared` → `FirelandsDomain` → `FirelandsApplication` → `FirelandsInfrastructure`.
- **Executables:** `auth`, `world`, `FirelandsDevTools`.
- **Optional target** `merge-migrations` runs `tools/merge_migrations.py` to refresh **`sql/bundled/`** (requires `python3`; optional scratch `sql/merged/`).
- **Includes:** `${PROJECT_SOURCE_DIR}/src` so headers use `#include <application/...>` style.

## SQL (`sql/`)

- Schema and incremental `.sql` files; **`DatabaseMigrator`** executes them in **lexicographic order** (prefix files with `0_`, `1_`, …).
- Typical split: auth schema, characters schema, world data fixes—URI in YAML selects which database receives statements (scripts often `CREATE DATABASE` / `USE`).
- Docker/deploy bundles: **`sql/bundled/*.sql`** (see `docker-compose.yml`).

## Tools (`src/tools/`, `tools/`)

- **`FirelandsDevTools`** (`src/tools/DevTools.cpp`) — CLI for accounts/realms against MariaDB; documented in [devtools.md](../devtools.md).
- **`tools/extractors/`** — MPQ/data extraction pipeline (StormLib); see [extractors.md](../extractors.md).
- **`tools/vmap/`** — map/vmap extraction helpers and tests; see [VMAP_EXTRACTION_PLAN.md](../VMAP_EXTRACTION_PLAN.md).

## Tests (`tests/`)

- **GoogleTest** targets under `tests/unit/` mirror layers: `application`, `domain`, `infrastructure`, `shared`, `tools`, `vmap`.
- Run via CTest after configuring CMake (`BUILD_TESTING`).

