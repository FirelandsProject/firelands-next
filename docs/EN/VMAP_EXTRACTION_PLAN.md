# VMap Extraction & Build Pipeline — Port Plan (WoW 4.3.4 / build 15595)

**Strategy:** Full C++ port from reference implementation source — zero wrapped binaries.  
**Goal:** Byte-identical output to the reference so the existing `IMapCollisionQueries` / VMapManager2 runtime can consume our files without modification.  
**Reference:** use a local clone of reference implementation for the tools and Collision code.

---

## 1. The three-tool pipeline (end-to-end)

```
WoW 4.3.4 Data/
      │  (MPQ + locale MPQs — already handled by MpqPatchChain)
      ▼
┌─────────────────────┐
│  firelands-map-     │  ← Tool 1  (ADT/WDT → .map tiles + DBC dump)
│    extractor        │
└────────┬────────────┘
         │  maps/          dbc/
         ▼
┌─────────────────────┐
│  firelands-vmap4-   │  ← Tool 2  (WMO/M2 geometry → Buildings/)
│    extractor        │
└────────┬────────────┘
         │  Buildings/dir_bin   Buildings/<model>.vmo-raw
         ▼
┌─────────────────────┐
│  firelands-vmap4-   │  ← Tool 3  (Buildings → vmaps/)
│    assembler        │
└────────┬────────────┘
         │  vmaps/*.vmtree   vmaps/*.vmtile   vmaps/*.vmo
         ▼
   Server worldserver reads vmaps/ via IMapCollisionQueries
```

**Run order is mandatory.** Tools 2 and 3 do not depend on `maps/` output directly but must follow Tool 1 in documentation to be clear.

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

## 3. Repository layout (new directories)

```
tools/
  extractors/          ← already exists (DBC / raw map extract + MpqPatchChain)
  vmap/
    common/            ← shared math, chunk reader, DBC reader reused by tools 1–3
    map_extractor/     ← Tool 1
    vmap4_extractor/   ← Tool 2
    vmap4_assembler/   ← Tool 3
```

All three tools link a new static library **`FirelandsVmapCommon`** and the existing **`FirelandsExtractCommon`** (for `MpqPatchChain`, `WowDataMpqList`). No linkage into `world`/`auth`.

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

Target binary: **`firelands-map-extractor`** (replaces / supersedes existing stub with full ADT logic).

| Our file | Ported from | Content |
|----------|-------------|---------|
| `AdtChunks.h` | `map_extractor/adt.h` | `AdtMcvt`, `AdtMclq`, `AdtMcnk`, `AdtMfbo`, `AdtLiquidInstance`, `AdtMh2o`; liquid constants |
| `WdtChunks.h` | `map_extractor/wdt.h` | `WdtMain` (64×64 presence grid) |
| `MapFileWriter.h/cpp` | `map_extractor/System.cpp` (write side) | Output binary format writer: `MapFileHeader`, AREA / MHGT / MLIQ / holes chunks |
| `LiquidTables.h/cpp` | `map_extractor/System.cpp` (DBC caches) | `LiquidMaterialEntry`, `LiquidObjectEntry`, `LiquidTypeEntry` + DBC loaders |
| `AdtConverter.h/cpp` | `map_extractor/System.cpp` (`ConvertADT`) | ADT → `.map` conversion logic |
| `MapExtractorMain.cpp` | `map_extractor/System.cpp` (`main`) | CLI, MPQ loading, WDT iteration, DBC/DB2 dump, camera M2 copy |

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

Target binary: **`firelands-vmap4-extractor`**

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

## 6. Phased delivery

### Phase A — Common library + BIH (prerequisite for phases B–D)
- [ ] `VMapMagic.h` — all constants.
- [ ] `Vec3.h` — Vec3, AaBox3, Quaternion.
- [ ] `Mat3.h` — `fromEulerAnglesZYX`, `toEulerAnglesXYZ`, `Quat * Mat3`, `Mat3 * Vec3`.
- [ ] `ChunkReader.h/cpp` — chunked file parser (`MVER` ver-18 guard).
- [ ] `MpqStream.h/cpp` — full-file MPQ load, seek, `flipcc`.
- [ ] `DbcReader.h/cpp` — WDBC reader.
- [ ] `ModelSpawn.h/cpp` — flags, `readFromFile/writeToFile`.
- [ ] `BoundingIntervalHierarchy.h/cpp` — build + `writeToFile/readFromFile`.
- [ ] CMake: `FirelandsVmapCommon` static lib.
- [ ] Unit tests: BIH round-trip (write → read → same bounds/tree); `ModelSpawn` serialization.

### Phase B — Tool 1: map extractor (`.map` + `.tilelist`)
- [ ] `AdtChunks.h` — all struct layouts.
- [ ] `LiquidTables.h/cpp` — DBC-driven liquid resolution.
- [ ] `AdtConverter.h/cpp` — ADT → `.map` (MHGT, MLIQ, AREA, holes).
- [ ] `MapExtractorMain.cpp` — CLI + MPQ loading + WDT 64×64 iteration.
- [ ] Integration test: extract a single known ADT tile; compare MHGT header fields and area chunk against reference output.

### Phase C — Tool 2: vmap4 extractor (`Buildings/`)
- [ ] `ModelHeaders.h`, `M2Model.h/cpp` — M2 collision mesh + `ConvertToVMAPModel`.
- [ ] `WmoRoot.h/cpp`, `WmoGroup.h/cpp` — WMO chunk parsers + group converter.
- [ ] `DoodadData.h`, `DoodadExtract.h/cpp`, `WmoInstanceExtract.h/cpp`.
- [ ] `AdtModelExtract.h/cpp`, `WdtModelExtract.h/cpp`.
- [ ] `UniqueIdGen.h/cpp`.
- [ ] `GameObjectExtract.h/cpp`.
- [ ] `VmapExtractorMain.cpp` — CLI + `ParsMapFiles`.
- [ ] Integration test: extract a small map; `dir_bin` has non-zero records; at least one `.vmo`-raw model file produced; header magic validates.

### Phase D — Tool 3: vmap4 assembler (`vmaps/`)
- [ ] `WorldModelRaw.h/cpp` — `GroupModel_Raw::Read`, `WorldModel_Raw::Read`.
- [ ] `WorldModel.h/cpp` — `GroupModel`, `WorldModel`, `WmoLiquid`, `writeFile`.
- [ ] `TileAssembler.h/cpp` — `readMapSpawns`, `calculateTransformedBound`, `convertWorld2`, `convertRawFile`, `exportGameobjectModels`.
- [ ] `AssemblerMain.cpp` — CLI defaults (`Buildings` → `vmaps`).
- [ ] Integration test: assemble a small map set; validate `.vmtree` magic + NODE/SIDX structure; validate at least one `.vmo` header; validate `.vmtile` nSpawns matches `dir_bin` record count for that tile.

### Phase E — Runtime wiring (server side)
- [ ] Port or adapt `VMapManager2` + `StaticMapTree` + `ModelInstance` into `src/infrastructure/world/` (or a new `src/infrastructure/collision/`).
- [ ] Remove `MapCollisionQueriesStub`, wire real `IMapCollisionQueries` implementation.
- [ ] `worldserver.yaml` — `Collision.DataRoot` path.
- [ ] End-to-end test: server loads vmaps for a map, raycast to ground returns plausible Z.

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

---

## 9. Testing strategy (by phase)

| Phase | Test type | What to check |
|-------|-----------|--------------|
| A | Unit | BIH: build 10 AABoxes, write, read, verify bounds + objects. |
| A | Unit | ModelSpawn: write one with MOD_HAS_BOUND, read back, field equality. |
| B | Integration | Extract map 0 tile (32,32); open output in hex editor; validate "MAPS" header, MHGT min/max plausible (near sea level). |
| C | Integration | `dir_bin` non-empty; at least one `.vmo`-raw file; open, check "VMAP048" at offset 0. |
| D | Integration | `vmaps/000.vmtree` exists; open, "VMAP_4.8" at offset 0, then "NODE". |
| D | Integration | `vmaps/000_32_32.vmtile` exists; nSpawns > 0. |
| E | Runtime | Server LOS query on a coordinate known to be blocked by a building returns false (manual test or automated with fixture). |

---

## 10. Decisions resolved

| Decision | Resolution |
|----------|------------|
| Port vs wrap | **Full C++ port** — no wrapped binaries. |
| One binary vs three | **Three separate executables** (mirrors ref; lower memory per process; clear separation of concerns). |
| G3D dependency | **Remove** — inline minimal math in `Mat3.h`. |
| Boost.Filesystem | **Remove** — use `std::filesystem`. |
| Interactive menu | Add same pattern as `firelands-extractors` to each tool's main. |

---

*Implementation starts with Phase A. Do not merge Phase C until Phase A unit tests pass. Do not run the assembler (Phase D) until Phase C produces a valid `Buildings/dir_bin`.*
