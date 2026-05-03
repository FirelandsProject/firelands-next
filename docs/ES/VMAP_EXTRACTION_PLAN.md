# Plan maestro — extractores de datos cliente y colisión (4.3.4 / 15595)

El plan **canónico y detallado** (checklist, diagramas, formatos binarios, riesgos) está en inglés para mantener una sola fuente al día con el código:

- **[docs/EN/VMAP_EXTRACTION_PLAN.md](../EN/VMAP_EXTRACTION_PLAN.md)**

## Resumen ejecutivo

| Fase | Qué es | Estado |
|------|--------|--------|
| **A** | Librería común `FirelandsVmapCommon` (BIH, DBC, MPQ stream, magics) | Hecho |
| **B** | `firelands-map-extractor-vmap` → `.map` + `.tilelist` + **`Cameras/`** | Implementado; paridad: test de bytes fijos + golden opcional (`VMAP_MAP_TILE_PARITY.md`) |
| **C** | `firelands-vmap4-extractor` → `Buildings/` | Portado; **CLI testeada** (`Vmap4ExtractorCliTests`); pendiente extract real → `dir_bin` / `VMAP048` en CI o receta manual |
| **D** | `firelands-vmap4-assembler` → `vmaps/` | **Implementado** (test de integración §9: `TileAssemblerIntegrationTests.cpp`) |
| **E** | Runtime: `VMapManager2` + sustituir `MapCollisionQueriesStub` | Por hacer |
| **F** | Generador **mmaps** (Recast/Detour) | Por hacer |
| **G** | TUI `firelands-extractors` + documentación de flujo único | Parcial — ya incluye map vmap + vmap4 extract/assemble; falta mmap gen. |
| **H** | Criterios de cierre (CI, smoke, sin stubs cuando hay datos) | Por definir al cerrar |

**Dos binarios “map”:** `firelands-map-extractor` (MPQ → archivos crudos `World/maps`) ≠ `firelands-map-extractor-vmap` (→ `maps/*.map` para colisión).

Para tareas concretas, edita y marca checkboxes en el documento EN enlazado arriba.
