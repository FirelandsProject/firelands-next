# Extractores de datos del cliente (4.3.4 / MPQ)

Las herramientas leen un folder retail de **Cataclysm** `World of Warcraft/Data` (archivos **MPQ**, no CASC) y extraen archivos usando **StormLib v9.26** (pinned en el `CMakeLists.txt` raíz).

## Requisitos

- Build con CMake del proyecto (misma toolchain que el resto de Firelands).
- Paquetes de desarrollo de **zlib** y **bzip2** (StormLib los usa cuando `STORM_USE_BUNDLED_LIBRARIES` está apagado).

## Binarios

| Target | Propósito |
|--------|-----------|
| **`firelands-extractors`** | **Menú interactivo** (recomendado): tablas `DBFilesClient` (.dbc / .db2), maps, listar MPQs. |
| `firelands-dbc-extractor` | Extrae `DBFilesClient\*.dbc` y `DBFilesClient\*.db2`; **sin argumentos** abre el mismo menú interactivo. |
| `firelands-map-extractor` | Extracción de assets de mapas; **sin argumentos** abre el mismo menú. |

Los artefactos quedan en `${CMAKE_BINARY_DIR}/bin/`.

## Modo interactivo

Ejecuta el shell interactivo (sin flags):

```bash
./firelands-extractors
```

O cualquiera de los extractores sin argumentos:

```bash
./firelands-dbc-extractor
./firelands-map-extractor
```

Verás un menú numerado (DBC+DB2, maps, listar orden MPQ, salir). Tras elegir, ingresa el **directorio `Data`** de WoW (debe existir) y, al extraer, un **directorio de salida** (se crea si no existe). `firelands-extractors --help` imprime un resumen.

## Modo no interactivo (scripts / CI)

Listar el orden de MPQs que usará StormLib (base + parches):

```bash
./firelands-dbc-extractor --data "/path/to/WoW/Data" --list-mpqs
```

Extraer todas las tablas `DBFilesClient` del cliente (`.dbc` y `.db2`):

```bash
./firelands-dbc-extractor --data "/path/to/WoW/Data" --out ./client-dbc
```

Extraer archivos relacionados a mapas:

```bash
./firelands-map-extractor --data "/path/to/WoW/Data" --out ./client-maps
```

## Orden de MPQ

Los MPQs bajo `Data` se ordenan para overlay de parches: el primero se abre como base y cada uno siguiente se aplica con `SFileOpenPatchArchive` para que el último “gane” si hay rutas internas repetidas. Para detalles: **`STORM_LIB_ROADMAP.md`**.

## Ver también

- `STORM_LIB_ROADMAP.md` — versiones, riesgos y milestones.
- StormLib oficial: [ladislav-zezula/StormLib](https://github.com/ladislav-zezula/StormLib).

