# Client data extractors (4.3.4 / MPQ)

Tools read a retail **Cataclysm** `World of Warcraft/Data` folder (MPQ archives, not CASC) and extract files using **StormLib v9.26** (pinned in the root `CMakeLists.txt`).

## Prerequisites

- CMake build of the project (same toolchain as the rest of Firelands).
- System **zlib** and **bzip2** development packages (StormLib links them when `STORM_USE_BUNDLED_LIBRARIES` is off).

## Binaries

| Target | Purpose |
|--------|---------|
| **`firelands-extractors`** | **Interactive console menu** (recommended): choose DBC / maps / list MPQs, then enter paths when prompted. |
| `firelands-dbc-extractor` | DBC extraction; **no arguments** opens the same interactive menu as above. |
| `firelands-map-extractor` | Map asset extraction; **no arguments** opens the same interactive menu. |

Build artifacts land under `${CMAKE_BINARY_DIR}/bin/` (see your generator output).

## Interactive mode (user-friendly)

Run the dedicated shell (no flags):

```bash
./firelands-extractors
```

Or run either extractor with **no arguments**:

```bash
./firelands-dbc-extractor
./firelands-map-extractor
```

You will see a numbered menu (DBC, maps, list MPQ order, exit). After choosing an action, enter the **WoW `Data` directory** (must exist) and, when extracting, an **output folder** (created if needed). `firelands-extractors --help` prints a short summary.

## Non-interactive mode (scripts / CI)

List the MPQ open order StormLib will use (base archive + patches):

```bash
./firelands-dbc-extractor --data "/path/to/WoW/Data" --list-mpqs
```

Extract all client DBCs:

```bash
./firelands-dbc-extractor --data "/path/to/WoW/Data" --out ./client-dbc
```

Extract map-related client files:

```bash
./firelands-map-extractor --data "/path/to/WoW/Data" --out ./client-maps
```

## MPQ ordering

Archives under `Data` are sorted for **patch overlay** order: the first file in the printed list is opened as the base archive; each following file is applied with `SFileOpenPatchArchive` so later entries override earlier ones for the same internal path. Details and milestones: **`STORM_LIB_ROADMAP.md`**.

## See also

- `STORM_LIB_ROADMAP.md` — versions, risks, and phase-2 server map tooling.
- Official StormLib: [ladislav-zezula/StormLib](https://github.com/ladislav-zezula/StormLib).
