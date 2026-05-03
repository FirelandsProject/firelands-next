# Plan maestro — extractores de datos cliente y colisión (4.3.4 / 15595)

El plan **canónico y detallado** (checklist, diagramas, formatos binarios, riesgos) está en inglés para mantener una sola fuente al día con el código:

- **[docs/EN/VMAP_EXTRACTION_PLAN.md](../EN/VMAP_EXTRACTION_PLAN.md)**

## Resumen ejecutivo

| Fase | Qué es | Estado |
|------|--------|--------|
| **A** | Librería común `FirelandsVmapCommon` (BIH, DBC, MPQ stream, magics) | Hecho |
| **B** | `firelands-map-extractor-vmap` → `.map` + `.tilelist` + carpeta **`Cameras/`** (M2 desde `CinematicCamera.dbc`) | Implementado; pendiente endurecer pruebas de paridad |
| **C** | `firelands-vmap4-extractor` → `Buildings/` | Portado; pendiente tests de integración y CLI |
| **D** | `firelands-vmap4-assembler` → `vmaps/` | **Implementado** (pendiente: tests de integración §9) |
| **E** | Runtime: `VMapManager2` + sustituir `MapCollisionQueriesStub` | Por hacer |
| **F** | Generador **mmaps** (Recast/Detour) | Por hacer |
| **G** | TUI `firelands-extractors` + documentación de flujo único | Parcial |
| **H** | Criterios de cierre (CI, smoke, sin stubs cuando hay datos) | Por definir al cerrar |

**Dos binarios “map”:** `firelands-map-extractor` (MPQ → archivos crudos `World/maps`) ≠ `firelands-map-extractor-vmap` (→ `maps/*.map` para colisión).

Para tareas concretas, edita y marca checkboxes en el documento EN enlazado arriba.
