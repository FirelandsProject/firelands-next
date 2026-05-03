# Herramientas, migraciones SQL, tests y build

## CMake / compilación (`CMakeLists.txt`)

- Proyecto **Firelands** en **C++17** con FetchContent para googletest, spdlog, MariaDB C/C++, nlohmann_json, yaml-cpp, Lua, StormLib.
- **Librerías:** `FirelandsShared` → `FirelandsDomain` → `FirelandsApplication` → `FirelandsInfrastructure`.
- **Ejecutables:** `auth`, `world`, `FirelandsDevTools`.
- Target opcional **`merge-migrations`** ejecuta `tools/merge_migrations.py` y actualiza **`sql/bundled/`** (requiere `python3`; scratch opcional `sql/merged/`).
- **Includes:** `${PROJECT_SOURCE_DIR}/src` para permitir `#include <application/...>`.

## SQL (`sql/`)

- Archivos `.sql` (schema e incrementales); **`DatabaseMigrator`** los ejecuta en **orden lexicográfico** (prefijos `0_`, `1_`, …).
- El URI del YAML define a qué base lógica se conectan los scripts (muchos scripts hacen `CREATE DATABASE` / `USE`).
- Bundles Docker/despliegue: **`sql/bundled/*.sql`** (ver `docker-compose.yml`).

## Herramientas (`src/tools/`, `tools/`)

- **`FirelandsDevTools`** — CLI para cuentas/reinos en MariaDB; ver [devtools.md](../devtools.md).
- **`tools/extractors/`** — extracción MPQ/datos cliente; ver [extractors.md](../extractors.md).
- **`tools/vmap/`** — pipeline de mapas/vmap; ver [VMAP_EXTRACTION_PLAN.md](../VMAP_EXTRACTION_PLAN.md).

## Tests (`tests/`)

- Tests con **GoogleTest** bajo `tests/unit/` por capa: `application`, `domain`, `infrastructure`, `shared`, `tools`, `vmap`.
- Se ejecutan con CTest tras configurar CMake (`BUILD_TESTING`).

