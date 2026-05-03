# Client Data & Collision Extractors — Master Plan (WoW 4.3.4 / build 15595)

**Strategy:** Full C++ port from the reference implementation — zero wrapped binaries for the collision pipeline.  
**Primary goal:** Reference-identical **server collision artifacts** (`.map`, `vmaps/`, `mmaps/`) so a ported **`VMapManager2` / mmap runtime** can replace `MapCollisionQueriesStub` without format hacks.  
**Secondary goal:** A coherent **extractor program**: MPQ tools (`StormLib`), DBC/raw-map CLI, TUI launcher, and documentation that describe one end-to-end workflow.  
**Reference:** local clone of the reference implementation (tools + `src/common/Collision`).

This document is the **single master plan** for “extractors” work. MPQ-only milestones are summarized in `docs/EN/STORM_LIB_ROADMAP.md` (Storm pin, phase-1 raw map extract); this file owns **collision + mmap + closure criteria**.

---

## 0. Scope — everything under “extractores”

| Area | CMake target(s) | Purpose | Status (high level) |
|------|-----------------|---------|---------------------|
| MPQ + patch order | `FirelandsExtractCommon` | `MpqPatchChain`, `WowDataMpqList` | **Done** |
| DBC / DB2 extract | `firelands-dbc-extractor` | `DBFilesClient` → output tree | **Done** |
| Raw client maps | `firelands-map-extractor` | `World/maps/**` from MPQs (WDT/ADT/WDL…) | **Done** (phase 1) |
| TUI launcher | `firelands-extractors` | FTXUI: DBC + raw maps today | **Partial** — must grow to drive full pipeline (§6 Phase G) |
| Shared vmap math I/O | `FirelandsVmapCommon` | Magic constants, BIH, DBC reader, MPQ stream, `ModelSpawn` | **Done** (see §6 Phase A) |
| Server `.map` + tilelist + **Cameras/** | `firelands-map-extractor-vmap` | Tool 1 — ADT/WDT → `maps/*.map`, `*.tilelist`; `CinematicCamera.dbc` → `Cameras/*.m2` | **Implemented** (consolidated sources; parity hardening in §6) |
| VMap4 extract | `firelands-vmap4-extractor` | Tool 2 — `Buildings/` | **Ported** (monolithic layout; modular split optional) |
| VMap4 assemble | `firelands-vmap4-assembler` | Tool 3 — `vmaps/` | **Implemented** (first port; integration tests pending) |
| MMAP generate | `firelands-mmap-generator` (name TBD) | Tool 4 — `mmaps/` (Recast/Detour) | **Not started** |
| Runtime collision | `world` | `IMapCollisionQueries` + data root | **Stub only** |

**Naming note (avoid confusion):** there are **two** “map extractors”. Only the **vmap** one produces server `.map` binaries.

| Binary | Location | Output |
|--------|----------|--------|
| `firelands-map-extractor` | `tools/extractors/` | Raw client files under `World/maps/…` |
| `firelands-map-extractor-vmap` | `tools/vmap/map_extractor/` | Server `maps/<id><yy><xx>.map` + `<id>.tilelist` |

---

## 1. Collision pipeline (end-to-end)

Raw MPQ extract (optional for Tool 2 if ADT paths resolve via the same MPQ chain) is separate from **server** `.map` generation.

```
WoW 4.3.4 Data/
      │  (MPQ + locale MPQs — MpqPatchChain / StormLib)
      ▼
┌──────────────────────────┐
│ firelands-map-extractor- │  ← Tool 1  ADT/WDT → server maps/*.map + *.tilelist
│ vmap                     │
└────────────┬─────────────┘
             │  maps/
             ▼
┌──────────────────────────┐
│ firelands-vmap4-         │  ← Tool 2  WMO/M2 → Buildings/
│ extractor                │
└────────────┬─────────────┘
             │  Buildings/dir_bin, *.vmo-raw, …
             ▼
┌──────────────────────────┐
│ firelands-vmap4-         │  ← Tool 3  Buildings/ → vmaps/
│ assembler                │
└────────────┬─────────────┘
             │  vmaps/*.vmtree, *.vmtile, *.vmo, GameObjectModels.dtree
             ▼
┌──────────────────────────┐
│ firelands-mmap-          │  ← Tool 4  maps/ + vmaps/ → mmaps/ (navmesh tiles)
│ generator                │
└────────────┬─────────────┘
             │  mmaps/
             ▼
   worldserver: VMap + Detour mmap loaders → real IMapCollisionQueries
```

**Run order:** Tool 1 → 2 → 3 → 4 for a full collision dataset. Tool 4 **requires** Tool 1 and Tool 3 outputs (and typically the same `maps/` layout the reference generator expects).

---

## 2. Magic strings and version constants (must match exactly)

Defined in reference `VMapDefinitions.h`; our port must use identical values.

| Constant | Value | Used in |
|----------|-------|---------|
| `RAW_VMAP_MAGIC` | `"VMAP048"` (8 bytes incl. `\0`) | Intermediate `Buildings/` files written by extractor, validated by assembler |
| `VMAP_MAGIC` | `"VMAP_4.8"` (8 bytes, no terminator in I/O) | Final `vmaps/` files written by assembler, validated by runtime |
| `GAMEOBJECT_MODELS` | `"GameObjectModels.dtree"` | GameObject model index file |
| `FILE_FORMAT_VERSION` | `18` | ADT/WDT `MVER` chunk version |
| `CONF_TargetBuild` | `15595` | Build gate for patch archive selection |
| `TILESIZE` | `533.33333f` | World units per ADT tile (critical for tile indexing) |

---

## 3. Repository layout (actual vs planned)

**As of this plan revision:**

```
tools/
  extractors/                    ← MPQ chain, DBC CLI, raw map CLI, FTXUI shell
    FirelandsExtractCommon
    firelands-dbc-extractor
    firelands-map-extractor      ← raw World/maps/* only (not server .map)
    firelands-extractors         ← TUI (must orchestrate full pipeline — Phase G)
  vmap/
    common/                      ← FirelandsVmapCommon (tools 1–3 + tests)
    map_extractor/               ← Tool 1 → firelands-map-extractor-vmap
    vmap4_extractor/             ← Tool 2 → firelands-vmap4-extractor
    vmap4_assembler/             ← Tool 3 → firelands-vmap4-assembler
  mmap/   (or vmap/mmap_generator/)
    …                            ← Tool 4 — to be added (Phase F)
```

**Linkage rule:** `FirelandsVmapCommon` (+ map extractor lib) must **not** link into `world` / `auth`. Tool 4 may depend on **Recast/Detour** as a `FetchContent` dependency; keep it scoped to the generator target only.

**Optional later:** link `firelands-map-extractor-vmap` against `FirelandsExtractCommon` to reuse one MPQ-open implementation instead of parallel Storm code in `MapExtractorTask.cpp` — only if parity tests prove identical file lists and patch order.

---

## 4. Component inventory and file mapping

### 4.1 `tools/vmap/common/` — shared library `FirelandsVmapCommon`

| Our file | Ported from | Responsibility |
|----------|-------------|----------------|
| `ChunkReader.h/cpp` | `map_extractor/loadlib.*` | Generic FOURCC + uint32-size chunk scanner; `ChunkedFile`, `FileChunk`; `MVER` ver-18 guard |
| `DbcReader.h/cpp` | `map_extractor/dbcfile.*` (identical copy in `vmap4_extractor/dbcfile.*`) | WDBC reader: `"WDBC"` magic, 4-byte fields, string block |
| `MpqStream.h/cpp` | `vmap4_extractor/mpqfile.*` | Whole-file MPQ loader with `read/seek/seekRelative`; `flipcc` helper |
| `Vec3.h` | `vmap4_extractor/vec3d.h` | `Vec3`, `AaBox3`, `Quaternion`; `fixCoordSystem(v) = (v.x, v.z, -v.y)` |
| `BoundingIntervalHierarchy.h/cpp` | `src/common/Collision/BoundingIntervalHierarchy.*` | BIH build + ray traversal + `writeToFile/readFromFile` |
| `ModelSpawn.h/cpp` | `src/common/Collision/Models/ModelInstance.*` | `ModelSpawn` struct, `MOD_*` flags, `readFromFile/writeToFile` |
| `VMapMagic.h` | `src/common/Collision/VMapDefinitions.h` | Centralises all magic-string and version constants |

**Key coordinate conventions (must be exact):**

- `fixCoordSystem(v)` → `(v.x, v.z, -v.y)` (applied to M2 bounding vertices at load time).
- Extra per-vertex transform in M2 VERT write: `y ← v.z`, `z ← -v.y`, `x ← v.x` (done inside `Model::ConvertToVMAPModel`; see §5.3).
- WMO/doodad world position: `fixCoords(pos) = Vec3(pos.z, pos.x, pos.y)`.
- Global WDT WMO offset: `+533.33333 * 32` on X and Y.

### 4.2 Tool 1 — `tools/vmap/map_extractor/`

Target binary: **`firelands-map-extractor-vmap`** (server `.map` + tilelist; **not** `tools/extractors/firelands-map-extractor`).

**Implemented layout** (functionally covers the rows below; names differ from an “ideal” split):

| Current module | Role |
|----------------|------|
| `AdtReader.h/cpp` | ADT chunk parse + liquid resolution → in-memory grid for writer |
| `WdtReader.h/cpp` | WDT 64×64 presence |
| `MapFileWriter.h/cpp` | `MAPS` header, AREA / MHGT / MLIQ / holes, tilelist writer |
| `MapExtractorTask.h/cpp` | MPQ open (world + locale), `Map.dbc` + liquid DBCs, per-map loop, deep-water ignore list, **camera M2 extract** |
| `map_extractor_main.cpp` | CLI `-d/-o/-b/-q` |

#### Cinematic cameras (`Cameras/`)

Reference **map_extractor** (`ExtractCameraFiles` / TCP `System.cpp`): read **`DBFilesClient\CinematicCamera.dbc`** from the **locale** archive; for each row, string field **1** is an internal path (often `Cameras\….mdx`); replace **`.mdx` → `.m2`**; open the file from the **world** MPQ handle (patch chain already applied; same resolution as `MpqStream` / `SFILE_OPEN_FROM_MPQ`); write to **`{output}/Cameras/{relative after Cameras\}`**, skipping rows with unknown layout or files already on disk.

Used by the world server for race/class intro and cinematic camera paths (`CMSG_NEXT_CINEMATIC_CAMERA` and related data).

**Reference-aligned file split** (optional refactor if readability suffers — not blocking):

| Idealized file | Ported from | Content |
|----------------|-------------|---------|
| `AdtChunks.h` | `map_extractor/adt.h` | Chunk struct layouts / constants |
| `LiquidTables.h/cpp` | `map_extractor/System.cpp` (DBC caches) | Liquid tables + loaders (today inside `MapExtractorTask` / `AdtReader`) |
| `AdtConverter` | `ConvertADT` | Could be peeled from `AdtReader` |

**Remaining optional gaps vs ref:** extra DB2 sidecars or locale-subfolder camera layout (335-era `Cameras/<locale>/`) — only if we need byte-identical folder layout to an older ref; **4.3.4 TCP** writes flat `{output}/Cameras/`.

#### Output format — `.map` (byte layout, must be exact)

```
map_fileheader  (mapMagic="MAPS", versionMagic=10, buildMagic=client_build,
                 offsets+sizes for: area, height, liquid, holes)
AREA chunk      ("AREA" + map_areaHeader { flags, gridArea } [+ uint16[16][16]])
MHGT chunk      ("MHGT" + map_heightHeader { flags, gridHeight, gridMaxHeight }
                  [+ int16[3][3] flight_box_max/min if HAS_FLIGHT_BOUNDS]
                  [+ height payload: floats or packed uint8/uint16])
MLIQ chunk      ("MLIQ" + map_liquidHeader { flags, offsetX/Y, width/height, liquidLevel }
                  [+ per-cell entry+flags] [+ height floats])
holes           (uint16[16][16] if any non-zero)
```

#### Tilelist — `{mapId:03}.tilelist`

```
"MAPS"  uint32:version=10  uint32:build  char[4096]:  '0'/'1' per 64×64 tile
```

#### Deep-water ignore list (hard-coded, must match ref)

Map 0 (Azeroth) and Map 1 (Thousand Needles) have specific `(x,y)` grid cells where `ignoreDeepWater=true`. Port the exact cell list from `IsDeepWaterIgnored()` in `System.cpp`.

### 4.3 Tool 2 — `tools/vmap/vmap4_extractor/`

Target binary: **`firelands-vmap4-extractor`**.

**Current port shape:** one executable built from `vmapexport.cpp`, `adtfile.cpp`, `wdtfile.cpp`, `wmo.cpp`, `model.cpp`, `mpqfile.cpp`, `gameobject_extract.cpp`, `QuatMath.cpp`, linked to `FirelandsVmapCommon`. The table below is the **logical** module map; splitting files to match names 1:1 is optional cleanup.

| Our file | Ported from | Content |
|----------|-------------|---------|
| `ModelHeaders.h` | `vmap4_extractor/modelheaders.h` | `ModelHeader` (packed M2 layout) |
| `M2Model.h/cpp` | `vmap4_extractor/model.*` | M2 collision mesh loader + `ConvertToVMAPModel()` |
| `WmoRoot.h/cpp` | `vmap4_extractor/wmo.*` (WMORoot + WMOGroup) | WMO root chunk reader (`MOHD`,`MOGN`,`MODS`,`MODN`,`MODD`), `ConvertToVMAPRootWmo()` |
| `WmoGroup.h/cpp` | same | WMO group chunk reader (`MOGP`,`MOPY`,`MOVI`,`MOVT`,`MOBA`,`MODR`,`MLIQ`), `ConvertToVMAPGroupWmo()` |
| `DoodadData.h` | `vmap4_extractor/wmo.h` | `WmoDoodadData` (Sets/Paths/Spawns/References) |
| `AdtModelExtract.h/cpp` | `vmap4_extractor/adtfile.*` | ADT chunk walk: `MMDX/MDDF`, `MWMO/MODF`; appends to `dir_bin` |
| `WdtModelExtract.h/cpp` | `vmap4_extractor/wdtfile.*` | WDT chunk walk: global `MWMO/MODF`; lazy `_obj0.adt` access |
| `DoodadExtract.h/cpp` | `vmap4_extractor/model.cpp` `Doodad::*` | `Extract()` and `ExtractSet()` namespace |
| `WmoInstanceExtract.h/cpp` | `vmap4_extractor/wmo.cpp` `MapObject::*` | `MapObject::Extract()` |
| `GameObjectExtract.h/cpp` | `vmap4_extractor/gameobject_extract.cpp` | `GameObjectDisplayInfo.dbc` scan → `temp_gameobject_models` |
| `UniqueIdGen.h/cpp` | `vmap4_extractor/vmapexport.cpp` `GenerateUniqueObjectId` | Stable ID mapping `(clientId, doodadId) → uniqueId` |
| `VmapExtractorMain.cpp` | `vmap4_extractor/vmapexport.cpp` `main` | CLI, MPQ chain, Map.dbc, parent-map tracking, `ParsMapFiles`, `ExtractWmo` |

#### Output — `Buildings/` directory

```
Buildings/
  dir_bin            ← spawn stream (no header; repeated per-spawn records)
  temp_gameobject_models  ← RAW_VMAP_MAGIC + records
  <PlainModelName>   ← RAW_VMAP_MAGIC + model-specific layout (see below)
```

#### `Buildings/dir_bin` record layout (no magic header)

Repeated until EOF:

```
uint32  mapId
uint8   flags     (MOD_M2=1, MOD_HAS_BOUND=2, MOD_PARENT_SPAWN=4)
uint8   adtId     (always 0 from extractor)
uint32  uniqueId
float[3] iPos    (fixCoords applied)
float[3] iRot    (degrees; ADT path: raw; WMO doodad set: computed from quat)
float   iScale
--- if MOD_HAS_BOUND (WMO instances) ---
float[3] iBound.low
float[3] iBound.high
--- always ---
uint32  nameLen
char[nameLen] name
```

#### Raw model file layout (`Buildings/<name>`, magic = `RAW_VMAP_MAGIC`)

**M2:**
```
char[8]  "VMAP048"
int32    nVertices
uint32   nGroups = 1
int32[3] zeros
float[6] AaBox3 (collisionBox from ModelHeader)
int32    liquidFlags = 0
"GRP "   wsize (int32) | nBranches=1 | uint32[1] nIndexes
"INDX"   wsize | nIndexes | uint16[nIndexes]   (winding fix: swap pairs at positions 1,4,7…)
"VERT"   wsize | nVertices | float[nVertices×3] (extra coord swap: y→v.z, z→-v.y)
```

**WMO root + groups (all in one file):**
```
char[8]  "VMAP048"
uint32   nVectors   ← patched AFTER writing all groups
uint32   nGroups    ← patched AFTER writing all groups
uint32   RootWMOID
[ per group: ConvertToVMAPGroupWmo block ]
```

**WMO group block:**
```
uint32  mogpFlags
uint32  groupWMOID
float[3] bbcorn1,  float[3] bbcorn2
uint32  liquidFlags
"GRP "  moba_size_grp | moba_batch | int32[moba_batch]  (batch indices from MOBA stride-12)
"INDX"  wsize | nIndexes | uint16[nIndexes]
"VERT"  wsize | nVertices | float[nVertices×3]
[ "LIQU"  wsize | groupLiquid | WMOLiquidHeader | heights | tileBytes ]
```

#### Critical WMO group logic

- **`MOGP` inner size is hard-capped at 68 bytes** (payload of MOGP header fields only; sub-chunks follow after).
- **Skip group if** `mogpFlags & 0x80` or `& 0x4000000` or group name is `"antiportal"`.
- **Skip WMO instance if** `MODF.Flags & 0x1` (destructible).
- **Skip file if** plain name has trailing `_NNN` pattern (= group sub-file, not root).
- **MMID/MWID not used:** `MDDF.Id` / `MODF.Id` index the N-th string in order of appearance in `MMDX` / `MWMO` scan. Port must match this behavior exactly.

### 4.4 Tool 3 — `tools/vmap/vmap4_assembler/`

Target binary: **`firelands-vmap4-assembler`**

| Our file | Ported from | Content |
|----------|-------------|---------|
| `WorldModelRaw.h/cpp` | `src/common/Collision/Maps/TileAssembler.*` (`WorldModel_Raw`, `GroupModel_Raw`) | `GroupModel_Raw::Read` (reads `INDX`/`VERT`/`GRP `/`LIQU` blocks); `WorldModel_Raw::Read` |
| `TileAssembler.h/cpp` | same (`TileAssembler::convertWorld2`, `readMapSpawns`, `convertRawFile`, `exportGameobjectModels`) | Full assembly pipeline |
| `WorldModel.h/cpp` | `src/common/Collision/Models/WorldModel.*` | `GroupModel`, `WorldModel`, `WmoLiquid`, `writeFile/readFile` |
| `AssemblerMain.cpp` | `vmap4_assembler/VMapAssembler.cpp` | CLI: paths, calls `TileAssembler::convertWorld2()` |

#### Output — `vmaps/` directory

```
vmaps/
  {mapId:03}.vmtree        ← global BIH over all map spawns
  {mapId:03}_{Y:02}_{X:02}.vmtile  ← per-tile spawn list
  <relative/path>.vmo      ← compiled WorldModel
  GameObjectModels.dtree   ← GO model index (if temp_gameobject_models present)
```

#### `.vmtree` layout (magic = `VMAP_4.8`)

```
char[8]  "VMAP_4.8"
char[4]  "NODE"
BIH::writeToFile blob:
  float[3] bounds.low
  float[3] bounds.high
  uint32   treeSize
  uint32[treeSize] tree
  uint32   count
  uint32[count] objects   ← primitive indices
char[4]  "SIDX"
uint32   mapSpawnsSize
{ uint32 spawnId, uint32 treeIndex }[mapSpawnsSize]
```

#### `.vmtile` layout

```
char[8]  "VMAP_4.8"
uint32   nSpawns
ModelSpawn[nSpawns]  (regular then MOD_PARENT_SPAWN bucket)
```

#### `.vmo` layout

```
char[8]  "VMAP_4.8"
"WMOD"  uint32:chunkSize=8  uint32:RootWMOID
"GMOD"  uint32:count  GroupModel::writeToFile × count
"GBIH"  BIH::writeToFile
```

**`GroupModel::writeToFile`:**
```
AABox  (float[6]: low xyz, high xyz)
uint32 iMogpFlags
uint32 iGroupWMOID
"VERT"  chunkSize | uint32:count | Vector3[count]
"TRIM"  chunkSize | uint32:count | MeshTriangle[count]  (3 × uint32 indices)
"MBIH"  BIH::writeToFile
"LIQU"  uint32:chunkSize | WmoLiquid::writeToFile
```

---

## 5. External dependencies for the port

| Dependency | Status | Notes |
|------------|--------|-------|
| **StormLib v9.26** | Already integrated | Reuse existing `MpqPatchChain` + `WowDataMpqList` |
| **zlib / bzip2** | Already integrated | No change |
| **G3D (g3dlite)** | **Must remove / replace** | Reference uses `G3D::Vector3`, `G3D::Matrix3`, `G3D::Quat`, `fromEulerAnglesZYX`, `toEulerAnglesXYZ`. Port these ops as minimal inline math in `Vec3.h` (already have `Vec3D`/`Quaternion` in extractor; extend). Do **not** add g3dlite as a dep. |
| **Boost.Filesystem** | Do **not** add | Replace with `std::filesystem` (already used in extractors). |
| **C++17 STL** | Available | `std::unordered_map/set`, `std::filesystem`, `std::thread` for assembler parallelism. |

**Key math operations to port from G3D (no external dep):**

```cpp
// WMO doodad composite transform (ExtractSet)
Matrix3 fromEulerAnglesZYX(float z, float y, float x);  // build rotation matrix
Vec3    rotate(Matrix3 const&, Vec3 const&);             // matrix × vec
void    toEulerAnglesXYZ(Matrix3 const&, float&x, float&y, float&z); // decompose
Quat    multiply(Quat const& q, Matrix3 const& m);       // Quat * Matrix3 → Quat
```

These are four functions totalling ~50 lines; implement in `tools/vmap/common/Mat3.h`.

---

## 6. Phased delivery (status + remaining work)

Use this section as the **authoritative checklist**. Update checkboxes when merging work.

### Phase A — Common library + BIH
- [x] `VMapMagic.h/cpp` — constants + `ReadAndValidateChunk`.
- [x] `Vec3.h` — Vec3, AaBox3, Quaternion (header-only).
- [x] `Mat3.h` — Euler / quat helpers (header-only).
- [x] `ChunkReader.h` — chunked parser + `MVER` guard (header-only; no `.cpp` required).
- [x] `MpqStream.h/cpp`.
- [x] `DbcReader.h/cpp`.
- [x] `ModelSpawn.h/cpp`.
- [x] `BoundingIntervalHierarchy.h/cpp`.
- [x] CMake: `FirelandsVmapCommon`.
- [x] Unit tests: `tests/unit/vmap/` — BIH, `ModelSpawn`, `Vec3`/`Mat3`, `MapFileWriter`, `QuatMath`.

### Phase B — Tool 1: server `.map` + `.tilelist` + `Cameras/` (`firelands-map-extractor-vmap`)
- [x] ADT parse + liquid resolution + height/liquid/area → `MapFileWriter`.
- [x] WDT iteration + per-tile ADT load + `IsDeepWaterIgnored` parity.
- [x] `MapExtractorTask` — world + locale MPQ chain, `Map.dbc` + liquid DBCs.
- [x] CLI `map_extractor_main.cpp`.
- [x] **Cinematic cameras:** `CinematicCamera.dbc` → extract listed M2 into `Cameras/` (DBC field 1, `.mdx`→`.m2`, world MPQ read).
- [x] **Parity / integration:** `MapExtractorParityTests` — deterministic 68-byte `MapFileWriter` golden; optional `0003232.golden.map` + `FIRELANDS_VMAP_PARITY_WOW_DATA` byte-compare for Azeroth tile (32,32). See `docs/EN/VMAP_MAP_TILE_PARITY.md` and `tests/fixtures/vmap/README.md`.
- [ ] **Optional:** extra DB2 dump paths if runtime tooling requires them.

### Phase C — Tool 2: vmap4 extractor (`firelands-vmap4-extractor` → `Buildings/`)
- [x] Executable + ref-derived sources (`vmapexport`, ADT/WDT/WMO/M2 paths).
- [x] **CLI parse:** `Vmap4ExtractorCli.{h,cpp}` + `Vmap4ExtractorCliTests.cpp` (`--help`, `-d/-o/-b/-q/-s/-l`, errors); `main` uses exit **0** (help), **1** (bad args / fatal extract).
- [ ] **Hardening (optional):** subprocess smoke on built binary; document `-d` = install root (contains `Data/`) vs Tool 1 `-d` + `/Data` append — clarify in operator doc only if operators get confused.
- [ ] **Integration test** (CI or manual recipe in doc): small map extract → non-empty `dir_bin` + at least one raw model with `RAW_VMAP_MAGIC`.

### Phase D — Tool 3: vmap4 assembler (`vmaps/`) — **blocking for mmap + runtime**
- [x] `tools/vmap/vmap4_assembler/` + CMake target **`firelands-vmap4-assembler`**.
- [x] Raw model read path — `GroupModel_Raw::Read`, `WorldModel_Raw::Read` (embedded in `TileAssembler.cpp`, ref-aligned).
- [x] `WorldModelWrite.h/cpp` — `GroupModel`, `WorldModel`, `WmoLiquid`, `writeFile` (assembler write subset; no runtime ray queries).
- [x] `TileAssembler.h/cpp` — `readMapSpawns`, `calculateTransformedBound`, `convertWorld2`, `convertRawFile`, `exportGameobjectModels`.
- [x] `assembler_main.cpp` — CLI: `[Buildings] [vmaps]` (defaults match ref).
- [x] **Integration test:** `.vmtree` / `.vmtile` / `.vmo` magic and structural checks per §9 (synthetic `Buildings/` in `TileAssemblerIntegrationTests.cpp`).

### Phase E — Runtime wiring (server) — **collision “done” for gameplay**
- [ ] Port `VMapManager2` + `StaticMapTree` + related collision types into `src/infrastructure/` (dedicated `collision/` subdir recommended).
- [ ] Replace `MapCollisionQueriesStub` with real `IMapCollisionQueries` (LoS / height / queries used by packets & scripts).
- [ ] `worldserver.yaml` — `Collision.DataRoot` (and document layout: `vmaps/` + optional `maps/` beside it).
- [ ] **E2E:** LoS blocked where a wall exists; ground height non-trivial on a known coordinate.

### Phase F — MMAP generator (`mmaps/`) — **pathfinding dataset**
- [ ] Add **`firelands-mmap-generator`** (or equivalent name): port reference **`mmaps_generator`** (Recast + Detour tile build).
- [ ] CMake: `FetchContent` (or submodule) for **RecastNavigation** matching ref expectations; isolate to this target.
- [ ] Inputs: Tool 1 `maps/`, Tool 3 `vmaps/`, same `Map.dbc` / tile bounds assumptions as ref.
- [ ] Output: `mmaps/` tree byte-compatible with ref (validate against ref output on 1–2 maps).
- [ ] Extend runtime (Phase E) with **Detour navmesh load** where `IMapCollisionQueries::IsNavMeshDataAvailable` becomes true.

### Phase G — Launcher, CLI ergonomics, operator docs — **“extractores cerrados” UX**
- [x] `firelands-extractors` TUI: **DBC**, **raw maps**, **list MPQs**, **map-extractor-vmap** (in-process), **vmap4-extractor** + **vmap4-assembler** (subprocess to `build/bin` tools). **mmap-generator** still pending (Phase F).
- [ ] Single **documented recipe** in `docs/EN/extractors.md`: paths, order, disk layout, troubleshooting (MPQ not found, locale, build id).
- [ ] Align `docs/EN/STORM_LIB_ROADMAP.md` §7 with this checklist (or add “see master plan §6” and avoid duplicate schedules).

### Phase H — Release criteria (“tema extractores cerrado”)
- [ ] All targets in §0 build on Linux + macOS CI (Windows when tier-1).
- [ ] At least one **automated** test from phases A–D and one **documented manual** mmap smoke (until CI has client fixtures).
- [ ] No remaining `Placeholder` / `Stub` for collision on paths where data exists (feature-flag OK if data missing).

---

## 7. Naming convention mapping (ref → firelands-next)

| Reference (ref) | Firelands-next | Notes |
|-----------------|---------------|-------|
| `ChunkedFile` | `ChunkedFile` | Keep same name; file in `vmap/common/` |
| `DBCFile` | `DbcReader` | Firelands style; same logic |
| `MPQFile` | `MpqStream` | Avoids confusion with StormLib concept |
| `Vec3D` | `Vec3` | Already have in extractor; align |
| `AaBox3D` | `AaBox3` | Same |
| `BoundingIntervalHierarchy` | `BoundingIntervalHierarchy` | Keep name; critical data structure |
| `TileAssembler` | `TileAssembler` | Unique enough |
| `WorldModel_Raw` | `WorldModelRaw` | C++ style |
| `GroupModel_Raw` | `GroupModelRaw` | C++ style |
| `WMORoot` | `WmoRoot` | Consistent casing |
| `WMOGroup` | `WmoGroup` | Consistent casing |
| `ADTFile` | `AdtFile` | Consistent casing |
| `WDTFile` | `WdtFile` | Consistent casing |
| `ModelSpawn` | `ModelSpawn` | Same |
| `MOD_M2 / MOD_HAS_BOUND / MOD_PARENT_SPAWN` | `kModelFlag{M2,HasBound,ParentSpawn}` | Use `constexpr` or scoped enum |
| `GenerateUniqueObjectId` | `UniqueIdGenerator::generate(clientId, doodadId)` | Class wrapper |
| `szWorkDirWmo` = `"./Buildings"` | Passed as CLI arg; default `"./Buildings"` | No globals |
| `fixCoordSystem(v)` = `(v.x, v.z, -v.y)` | `CoordConv::fixClientCoord(v)` | Document axis meaning |
| `fixCoords(v)` = `(v.z, v.x, v.y)` | `CoordConv::fixWorldPlacement(v)` | WMO/doodad world position |

---

## 8. Risks and mitigations

| Risk | Mitigation |
|------|------------|
| **MMID/MWID not used** in ref (MDDF/MODF Id = scan order) | Port exactly as ref; document; test with a real ADT that has multiple model types. |
| **MOGP inner size hard-capped at 68** | Byte-level test: read a real group file with a short MOGP, verify group data parsed correctly. |
| **M2 winding fix** (swap pair at positions 1,4,7...) | Unit test `ConvertToVMAPModel` on a known M2 fragment; check triangle winding. |
| **Liquid resolution (DBC → type id)** | Port `GetLiquidTypeId` with the `(id-1) & 3` and `mogpFlags & 0x80000` branches verbatim. |
| **BIH object order must match SIDX table** | Integration test: build a small tree, write, read back, verify `spawnId[i] == spawn.ID[i]`. |
| **G3D math removal** | Validate `fromEulerAnglesZYX` against a known WMO transform; compare output `dir_bin` bytes for one doodad set. |
| **Deep-water ignore list** | Copy exact grid cell list from `IsDeepWaterIgnored()` verbatim; table-driven, not procedural. |
| **`dir_bin` has no magic** | Assembler relies on sequential read until EOF; ensure writer always closes cleanly and never leaves partial records. |
| **Recast / Detour version skew** | Pin the same Recast commit as ref; diff one `.mmtile` header after generator port. |
| **MMAP build without vmaps** | Enforce CLI validation: refuse to run Tool 4 if `vmaps/` or `maps/` layout missing. |

---

## 9. Testing strategy (by phase)

| Phase | Test type | What to check |
|-------|-----------|---------------|
| A | Unit | BIH: build 10 AABoxes, write, read, verify bounds + objects. |
| A | Unit | ModelSpawn: write one with MOD_HAS_BOUND, read back, field equality. |
| B | Integration | Extract map 0 tile (32,32); validate `"MAPS"` header; MHGT min/max plausible; optional byte-diff vs ref. |
| B | Integration | After run, `Cameras/` contains `.m2` files; count roughly matches ref extractor for same client. |
| C | Integration | `dir_bin` non-empty; at least one `.vmo`-raw file; `"VMAP048"` at offset 0. |
| D | Integration | `vmaps/000.vmtree` exists; `"VMAP_4.8"` at offset 0, then `"NODE"`. |
| D | Integration | `vmaps/000_32_32.vmtile` exists; `nSpawns > 0`. |
| E | Runtime | LoS / height: blocked vs open samples; graceful behavior when `DataRoot` missing. |
| F | Integration | One mmap tile generated; runtime reports nav data available; optional poly walk test. |
| G | Manual | TUI can drive full pipeline or documents equivalent shell script. |
| H | CI | `ctest` runs vmap units; CMake exposes all extractor targets. |

---

## 10. Decisions resolved

| Decision | Resolution |
|----------|------------|
| Port vs wrap | **Full C++ port** — no wrapped binaries. |
| Collision tools | **Four executables** — map (server) → vmap extract → vmap assemble → mmap generate (plus existing DBC/raw-map tools in `tools/extractors/`). |
| G3D dependency | **Remove** — inline minimal math in `Mat3.h`. |
| Boost.Filesystem | **Remove** — use `std::filesystem`. |
| Interactive launcher | Prefer **`firelands-extractors`** (TUI) for operators; each tool remains invocable CLI-only for scripts (`--help` / flags). Full pipeline wiring = Phase G. |

---

### Implementation order (guard rails)

1. Phases **A–C** are largely in place; keep unit tests green when changing shared code.  
2. Do **not** ship or rely on **Phase F** until **Phase D** produces valid `vmaps/` from real `Buildings/`.  
3. Do **not** remove `MapCollisionQueriesStub` until **Phase E** loads real data and tests cover failure modes.  
4. Treat **Phase H** as the definition of “extractores cerrados” for release branches.
