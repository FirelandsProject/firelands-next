# Server `.map` tile parity (Phase B)

This note complements **Phase B** in `VMAP_EXTRACTION_PLAN.md`: locking the **MapFileWriter** output layout and optionally comparing one real ADT tile to a **reference map_extractor** binary.

## 1. Deterministic writer lock (always in CI)

`MapExtractorParityTests.WriterFlatHeight100MatchesGoldenBytes` writes a synthetic `AdtGridData` (all heights `100`, default areas, no liquid) and compares the full file to a **68-byte** golden blob. Any intentional format change must update that test and this section.

## 2. Optional: one tile vs reference extractor

Goal: same client build (**15595**), same ADT (**Azeroth 32×32**), same `MapFileWriter` options → **identical bytes** to the reference `map_extractor` unless we document an exception.

### Generate the golden file

1. Run the reference **map_extractor** against a 4.3.4 `Data/` tree.
2. Locate `maps/0003232.map` (map **0**, tile **Y=32**, **X=32**).
3. Copy to `tests/fixtures/vmap/0003232.golden.map` in this repo.

### Run the Firelands check

```bash
export FIRELANDS_VMAP_PARITY_WOW_DATA="/absolute/path/to/WoW/Data"
ctest --test-dir build -R MapExtractorParity.OptionalGoldenTileMatchesReferenceExtractor
```

`ExtractSingleServerMapTile()` opens the same MPQ chain as the full task and writes exactly that tile for comparison.

### If `cmp` shows differences

Document here (date + reason), for example:

| Offset / region | Cause | Acceptable |
|-----------------|-------|------------|
| *(none yet)* | — | — |

Until a golden is checked in, the optional test **skips** in CI (no WoW client in the tree).

## 3. Related tests

- `tests/unit/vmap/MapFileWriterTests.cpp` — packing flags and header offsets.
- `tests/unit/vmap/MapExtractorParityTests.cpp` — golden blob + optional reference file.
