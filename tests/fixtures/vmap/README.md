# VMap / server `.map` parity fixtures

## `0003232.golden.map` (optional)

Byte-for-byte regression against another **map_extractor** build (same client 4.3.4 / 15595):

1. Extract one tile with the reference tool: **map id 0 (Azeroth), tile X=32, Y=32** → file typically named like `0003232.map` (pattern `%03u%02u%02u.map` = id, **Y**, **X**).
2. Copy that file here as `0003232.golden.map`.
3. Run tests with your WoW `Data` folder:

```bash
export FIRELANDS_VMAP_PARITY_WOW_DATA="/path/to/WoW/Data"
ctest --test-dir build -R MapExtractorParity.OptionalGoldenTileMatchesReferenceExtractor
```

If bytes differ, compare with `cmp -l` / hex editor and record **acceptable deltas** in `docs/EN/VMAP_MAP_TILE_PARITY.md` (float packing, liquid edge cases, etc.).

The committed **deterministic** test (`WriterFlatHeight100MatchesGoldenBytes`) does not need a client or golden file.
