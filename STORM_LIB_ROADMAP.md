# StormLib roadmap — DBC & map extractors (WoW 4.3.4 Cataclysm, build 15595)

This document is the **implementation plan** for game data extractors in **firelands-next**, aligned with **MPQ-based** Cataclysm clients (not CASC). Parity reference: **`firelands-cata-ref`** (FirelandsCore 4.3.4) when available locally.

---

## 1. Goals & scope

| Deliverable | Meaning for 4.3.4 |
|-------------|-------------------|
| **DBC extractor** | Open client **MPQ** archives, apply correct **patch order**, extract `DBFilesClient\*.dbc` (and locale variants where applicable) to an output tree for the emulator or SQL conversion. |
| **Map extractor (phase 1)** | Extract raw world assets from MPQs: `*.wdt`, `*.adt`, `*.wdl`, and related paths under `World\maps\`. |
| **Map extractor (phase 2, optional)** | Generate server binaries (`.map` / vmap / mmap) matching **ref** tooling — larger scope, separate milestone. |

**StormLib** covers **MPQ only** (open, find, read). Parsing **DBC** / **ADT** internals is our code (or ported from ref).

---

## 2. Library versions (pin explicitly)

### StormLib

- **Official source:** [ladislav-zezula/StormLib](https://github.com/ladislav-zezula/StormLib).
- **Policy:** Pin a **release tag** in CMake (`FetchContent` or submodule). **Do not** track `master`.
- **Rationale:** Cataclysm retail MPQs use **MPQ format v3/v4** (v4 common on final client). StormLib **9.x** releases include the required reader support.
- **Validation:** After integration, open at least: one large `patch-*.MPQ`, `expansion.MPQ`, and optionally `locale-xxXX.MPQ`.

### Zlib / BZip2

- StormLib needs **zlib** and **bzip2** for typical Blizzard-compressed blocks.
- Project already uses **ZLIB**; add **BZip2** in a way consistent with StormLib’s CMake (system or bundled per StormLib docs).
- Avoid mixed zlib versions between StormLib and our binaries on macOS.

### Optional StormLib features

- Strong crypto / signing (e.g. libtomcrypt): **not** required for normal read-only extraction; enable only if a specific archive fails to open.

### Toolchain

- **C++17**, CMake ≥ 3.10 (project baseline). Extractors as **standalone executables** — do **not** link StormLib into `world` / `auth` unless there is a deliberate runtime MPQ requirement.

---

## 3. Functional reference (4.3.4)

- **Patch order:** Match ref: base → expansion → `patch-*` → locale archives so the **last** archive in the chain wins for a given internal path.
- **Paths:** Blizzard-style separators (`\` in listings; normalize in code). Key roots:
  - DBC: `DBFilesClient\*.dbc`
  - Maps: `World\maps\<Map>\<Map>.wdt`, `World\maps\<Map>\<Map>_<tileX>_<tileY>.adt`, etc.
- **Client layout:** `World of Warcraft\Data\*.MPQ` plus optional loose files; CLI should accept **`--wow` / path to `Data`** and, if matching ref, **`component.wow-data.txt`** (or equivalent) for load order.

---

## 4. Technical design

### 4.1 Shared MPQ module

- Thin C++ wrapper: open archive, masked find (`SFileFindFirst` / …), `SFileOpenFileEx`, read to memory or stream to disk.
- Path normalization (`\` / `/`, case) for internal file names.

### 4.2 DBC extractor

- Input: `Data` directory or explicit MPQ list.
- Output: mirror under `DBFilesClient/` (configurable root).
- Optional: `--only Spell.dbc,...` for quick runs.

### 4.3 Map extractor (phase 1)

- Map list: allowlist or derive from ref / known 4.3.4 map set; optionally discover from `World\maps\` listings inside archives.
- Extract WDT/ADT/WDL per map into `maps/<Map>/...`.

### 4.4 Map extractor (phase 2)

- Port or align with ref: ADT → server `.map`, then vmap/mmap pipeline for real `IMapCollisionQueries` — **separate PR / milestone**.

---

## 5. Repository integration

- **CMake:** `tools/extractors/` (or `src/tools/extractors/`) with `FetchContent` for StormLib, e.g. `STORM_BUILD_TESTS=OFF`.
- **Targets:** e.g. `firelands-dbc-extractor`, `firelands-map-extractor`.
- **Docs:** `docs/extractors.md` — commands, prerequisites, pinned StormLib tag (update when bumping).

---

## 6. Risks & mitigations

| Risk | Mitigation |
|------|------------|
| MPQ v4 / HET-BET tables | Pin StormLib **9.x** tag; test with real 4.3.4 MPQs. |
| Wrong patch order → wrong DBC | Integration tests; spot-check vs ref output for a few tables. |
| macOS link quirks (zlib/bzip2) | Single CMake dependency graph; verify `otool -L` on built tools. |
| Scope creep on “map extractor” | Phase 1 = **raw extract** only until phase 2 is scheduled. |

---

## 7. Execution order

1. Add StormLib (**pinned tag**) + BZip2 + Zlib in CMake.
2. MPQ wrapper + minimal open/find test (synthetic or small MPQ).
3. **DBC extractor** + override-order test (two fixture MPQs, same internal path).
4. **Map extractor phase 1** (WDT/ADT extract).
5. Manual validation vs `firelands-cata-ref` with a real 4.3.4 `Data` folder.
6. (Optional) Phase 2 server map / vmap / mmap.

---

## 8. Pinned versions log (update when changed)

| Component | Version / tag | Notes |
|-----------|----------------|-------|
| StormLib | **v9.26** (`FetchContent` URL zip) | Official GitHub `refs/tags/v9.26` |
| Zlib | System / CMake `find_package` | Already required by root project |
| BZip2 | System / CMake `find_package` | Required for StormLib decompression |

---

*See root `CMakeLists.txt` (`FetchContent` block `stormlib`) and `docs/extractors.md` for CLI usage.*
