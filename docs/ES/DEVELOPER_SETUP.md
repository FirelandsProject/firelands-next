# Guía de Configuración para Desarrolladores

Esta guía ayuda a los nuevos desarrolladores a configurar su entorno para trabajar en Firelands, un emulador de World of Warcraft Cataclysm (4.3.4).

## Prerequisites

### Software Requerido

| Software | Versión | Propósito |
|----------|---------|-----------|
| **Compilador C++** | Compatible con C++17 (Clang 15+, GCC 11+, MSVC 2022+) | Compilar el proyecto |
| **CMake** | 3.20+ | Sistema de construcción |
| **Ninja** | Última versión | Generador de compilación rápido (recomendado) |
| **Git** | Cualquier versión reciente | Control de versiones |
| **Python** | 3.8+ | Script de merge SQL y herramientas |
| **MariaDB** o **MySQL** | 8.0+ | Base de datos (opcional para desarrollo, usa Docker) |

### Software Opcional

| Software | Propósito |
|----------|---------|
| **ccache** | Caché de compilación para acelerar rebuilds |
| **Docker Desktop** | Ejecutar base de datos local |
| **IDE** (VSCode, CLion, etc.) | Edición de código con soporte compile_commands.json |

### Instalación de Prerequisites (macOS)

```bash
# Install Homebrew if not installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required tools
brew install cmake ninja python3 git mariadb

# Optional: ccache for faster rebuilds
brew install ccache

# Optional: Docker Desktop for local database
# Download from https://www.docker.com/products/docker-desktop
```

### Instalación de Prerequisites (Linux - Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y cmake ninja-build python3 git mariadb-server libmariadb-dev zlib1g-dev libbz2-dev liblua5.4-dev

# Optional: ccache
sudo apt install -y ccache
```

### Instalación de Prerequisites (Windows)

1. Instalar **Visual Studio 2022** (o posterior) con "Desktop development with C++"
2. Instalar **CMake** desde https://cmake.org/download
3. Instalar **Ninja** desde https://ninja-build.org
4. Instalar **Docker Desktop** para base de datos local

## Clonar el Proyecto

```bash
git clone https://github.com/firelands-project/firelands-next.git
cd firelands-next
```

## Configuración de la Base de Datos

### Opción 1: Docker (Recomendado)

```bash
# Start MariaDB container
docker-compose up -d db

# Verify it's running
docker-compose ps
```

La base de datos estará disponible en:
- **Host**: localhost:3306
- **User**: root / firelands
- **Password**: root / firelands
- **Databases**: firelands_auth, firelands_characters, firelands_world

### Opción 2: MariaDB Local

```bash
# On macOS with Homebrew
brew services start mariadb

# On Linux
sudo systemctl start mariadb

# Create databases and user
mysql -u root -p
CREATE DATABASE firelands_auth;
CREATE DATABASE firelands_characters;
CREATE DATABASE firelands_world;
CREATE USER 'firelands'@'localhost' IDENTIFIED BY 'firelands';
GRANT ALL PRIVILEGES ON firelands_auth.* TO 'firelands'@'localhost';
GRANT ALL PRIVILEGES ON firelands_characters.* TO 'firelands'@'localhost';
GRANT ALL PRIVILEGES ON firelands_world.* TO 'firelands'@'localhost';
FLUSH PRIVILEGES;
```

## Compilar el Proyecto

### Configurar con CMake

```bash
# Create build directory
mkdir -p build
cd build

# Configure with Ninja generator
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..

# Optional: Enable tests
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFIRELANDS_BUILD_TESTS=ON ..
```

### Compilar Targets

```bash
# Build everything
ninja

# Build specific targets
ninja auth           # Authentication server
ninja world          # World server
ninja FirelandsDevTools  # DevTools CLI
```

### Salida de Compilación

Los binarios estarán en `build/bin/`:
- `auth` - Servidor de autenticación
- `world` - Servidor de mundo
- `FirelandsDevTools` - Herramientas de desarrollo

## Ejecutar los Servidores

### Servidor de Autenticación

```bash
# Run auth server
./build/bin/auth

# Or with custom config
FIRELANDS_AUTH_CONFIG=/path/to/config.yaml ./build/bin/auth
```

### Servidor de Mundo

```bash
# Run world server
./build/bin/world

# Or with custom config
FIRELANDS_WORLD_CONFIG=/path/to/config.yaml ./build/bin/world
```

### Archivos de Configuración

Archivos de configuración por defecto en la raíz del proyecto:
- `authserver.yaml` - Configuración del servidor de autenticación
- `worldserver.yaml` - Configuración del servidor de mundo

## Ejecutar Tests

```bash
# Configure with tests enabled
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFIRELANDS_BUILD_TESTS=ON ..

# Build
ninja

# Run all tests
ctest --test-dir build

# Run specific test suite
ctest --test-dir build -R unit
ctest --test-dir build -R domain
ctest --test-dir build -R application
```

## Estructura del Proyecto

```
firelands-next/
├── src/
│   ├── shared/          # Utilidades comunes (Logger, Config, red)
│   ├── domain/           # Modelos de dominio e interfaces de repositorio
│   ├── application/      # Casos de uso y servicios
│   ├── infrastructure/  # Implementaciones de base de datos, red, scripts
│   ├── auth/             # Ejecutable del servidor de autenticación
│   ├── world/            # Ejecutable del servidor de mundo
│   └── tools/           # Ejecutable DevTools
├── sql/
│   ├── init/             # Schema base
│   ├── migrations/      # Migraciones incrementales
│   └── bundled/         # SQL merging para Docker
├── data/                 # Client DBC/Maps (no está en el repo)
├── docs/                 # Documentación
├── tests/                # Tests unitarios GoogleTest
└── tools/                # Scripts de construcción/utilidades
```

## Integración con IDE

### VSCode

El proyecto incluye `.clangd` para IntelliSense. Abre la carpeta del proyecto en VSCode e instala la extensión "C/C++".

### CLion

Abre el proyecto y selecciona el `CMakeLists.txt` existente como modelo. Detectará automáticamente el directorio de compilación.

### Compile Commands

CMake genera `compile_commands.json` para integración con IDE:
```bash
ls -la compile_commands.json  # Symlink a build/
```

## Tareas Comunes

### Agregar una Nueva Migración

1. Crear archivo SQL en `sql/migrations/` con prefijo para ordenamiento:
   ```
   sql/migrations/001_mi_caracteristica.sql
   ```

2. El `DatabaseMigrator` ejecuta los archivos en orden lexicográfico al iniciar el servidor.

3. Rebuild: `ninja`

### Agregar un Nuevo Comando

1. Definir permiso en `src/shared/game/Permissions.h`
2. Registrar comando en `src/application/services/CommandService.cpp`
3. Implementar el método handler
4. Agregar soporte de base de datos si es necesario

### Agregar un Nuevo Archivo DBC

1. Colocar archivos `.dbc` o `.db2` en `data/dbc/`
2. Crear clase lectora en `src/infrastructure/dbc/` o `src/shared/dbc/`
3. Agregar a la secuencia de carga al inicio

### Ejecutar DevTools

```bash
# Crear cuenta
./build/bin/FirelandsDevTools account <username> <password>

# Crear reino
./build/bin/FirelandsDevTools realm <id> <name> <address> <port>
```

## Solución de Problemas

### Errores de Compilación

- **Faltan dependencias**: Ejecutar `cmake` de nuevo después de instalar nuevas librerías
- **Errores PCH**: Limpiar compilación con `rm -rf build && cmake -G Ninja ...`

### Errores de Conexión a la Base de Datos

- Verificar que Docker esté ejecutándose: `docker-compose ps`
- Verificar que las credenciales en los archivos de configuración coincidan con docker-compose.yml

### Problemas de Conexión del Cliente

- Asegurarse de que la versión del cliente sea 4.3.4 (build 15595)
- Revisar que el firewall permita conexiones en los puertos 3724 (auth) y 8085 (world)

## Próximos Pasos

- Leer [Resumen de Arquitectura](modules/README.md) para entender la arquitectura hexagonal
- Revisar [Administración GM](modules/gm-administration.md) para comandos de staff
- Consultar [Esquema de Base de Datos](DATABASE_SCHEMA.md) para documentación SQL
- Ver [Scripting LUA](LUA_SCRIPTING.md) para scripting de gameplay
- Leer [Protocolo](PROTOCOL.md) para documentación de paquetes/opcodes

## Obtener Ayuda

- Abrir un issue en GitHub
- Revisar la documentación existente en `docs/`
- Revisar la implementación de referencia en `firelands-cata-ref/`