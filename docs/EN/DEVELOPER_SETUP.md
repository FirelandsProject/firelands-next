# Developer Setup Guide

This guide helps new developers set up their environment to work on Firelands, a World of Warcraft Cataclysm (4.3.4) emulator.

## Prerequisites

### Required Software

| Software | Version | Purpose |
|----------|---------|---------|
| **C++ Compiler** | C++20 compatible (Clang 15+, GCC 12+, MSVC 2022 17.4+) | Building the project |
| **CMake** | 3.20+ | Build system configuration |
| **Ninja** | Latest | Fast build generator (recommended) |
| **Git** | Any recent version | Version control |
| **Python** | 3.8+ | SQL merge script and tooling |
| **MariaDB** or **MySQL** | 8.0+ | Database (optional for dev, uses Docker) |

### Optional Software

| Software | Purpose |
|----------|---------|
| **ccache** | Build cache to speed up rebuilds |
| **Docker Desktop** | Running local database |
| **IDE** (VSCode, CLion, etc.) | Code editing with compile_commands.json support |

### Installing Prerequisites (macOS)

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

### Installing Prerequisites (Linux - Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y cmake ninja-build python3 git mariadb-server libmariadb-dev zlib1g-dev libbz2-dev liblua5.4-dev

# Optional: ccache
sudo apt install -y ccache
```

### Installing Prerequisites (Windows)

1. Install **Visual Studio 2022** (or later) with "Desktop development with C++"
2. Install **CMake** from https://cmake.org/download
3. Install **Ninja** from https://ninja-build.org
4. Install **Docker Desktop** for local database

## Project Clone

```bash
git clone https://github.com/firelands-project/firelands-next.git
cd firelands-next
```

## Database Setup

### Option 1: Docker (Recommended)

```bash
# Start MariaDB container
docker-compose up -d db

# Verify it's running
docker-compose ps
```

The database will be available at:
- **Host**: localhost:3306
- **User**: root / firelands
- **Password**: root / firelands
- **Databases**: firelands_auth, firelands_characters, firelands_world

### Option 2: Local MariaDB

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

## Building the Project

### Configure with CMake

```bash
# Create build directory
mkdir -p build
cd build

# Configure with Ninja generator
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..

# Optional: Enable tests
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFIRELANDS_BUILD_TESTS=ON ..
```

### Build Targets

```bash
# Build everything
ninja

# Build specific targets
ninja auth           # Authentication server
ninja world          # World server
ninja FirelandsDevTools  # DevTools CLI
```

### Build Output

Binaries will be in `build/bin/`:
- `auth` - Authentication server
- `world` - World server
- `FirelandsDevTools` - Development tools

## Running the Servers

### Authentication Server

```bash
# Run auth server
./build/bin/auth

# Or with custom config
FIRELANDS_AUTH_CONFIG=/path/to/config.yaml ./build/bin/auth
```

### World Server

```bash
# Run world server
./build/bin/world

# Or with custom config
FIRELANDS_WORLD_CONFIG=/path/to/config.yaml ./build/bin/world
```

### Configuration Files

Default config files in project root:
- `authserver.yaml` - Auth server configuration
- `worldserver.yaml` - World server configuration

## Running Tests

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

## Project Structure

```
firelands-next/
├── src/
│   ├── shared/          # Common utilities (Logger, Config, network)
│   ├── domain/           # Domain models and repository interfaces
│   ├── application/      # Use cases and services
│   ├── infrastructure/  # Database, network, script implementations
│   ├── auth/             # Auth server executable
│   ├── world/            # World server executable
│   └── tools/           # DevTools executable
├── sql/
│   ├── init/             # Base schema
│   ├── migrations/      # Incremental migrations
│   └── bundled/         # Merged SQL for Docker
├── data/                 # Client DBC/Maps (not in repo)
├── docs/                 # Documentation
├── tests/                # GoogleTest unit tests
└── tools/                # Build/utility scripts
```

## IDE Integration

### VSCode

The project includes `.clangd` for IntelliSense. Open the project folder in VSCode and install the "C/C++" extension.

### CLion

Open the project and select the existing `CMakeLists.txt` as the project model. It will auto-detect the build directory.

### Compile Commands

CMake generates `compile_commands.json` for IDE integration:
```bash
ls -la compile_commands.json  # Symlink to build/
```

## Common Tasks

### Adding a New Migration

1. Create SQL file in `sql/migrations/` with prefix for ordering:
   ```
   sql/migrations/001_my_feature.sql
   ```

2. The `DatabaseMigrator` runs files in lexicographic order on server startup.

3. Rebuild: `ninja`

### Adding a New Command

1. Define permission in `src/shared/game/Permissions.h`
2. Register command in `src/application/services/CommandService.cpp`
3. Implement handler method
4. Add database support if needed

### Adding a New DBC File

1. Place `.dbc` or `.db2` files in `data/dbc/`
2. Create reader class in `src/infrastructure/dbc/` or `src/shared/dbc/`
3. Add to startup loading sequence

### Running DevTools

```bash
# Create account
./build/bin/FirelandsDevTools account <username> <password>

# Create realm
./build/bin/FirelandsDevTools realm <id> <name> <address> <port>
```

## Troubleshooting

### Build Errors

- **Missing dependencies**: Run `cmake` again after installing new libraries
- **PCH errors**: Clean build with `rm -rf build && cmake -G Ninja ...`

### Database Connection Errors

- Verify Docker is running: `docker-compose ps`
- Check credentials in config files match docker-compose.yml

### Client Connection Issues

- Ensure client version matches 4.3.4 (build 15595)
- Check firewall allows connections on ports 3724 (auth) and 8085 (world)

## Next Steps

- Read [Architecture Overview](modules/README.md) to understand the hexagonal architecture
- Check [GM Administration](modules/gm-administration.md) for staff commands
- Review [Database Schema](DATABASE_SCHEMA.md) for SQL documentation
- See [LUA Scripting](LUA_SCRIPTING.md) for gameplay scripting
- Read [Protocol](PROTOCOL.md) for packet/opcode documentation

## Getting Help

- Open an issue on GitHub
- Check existing documentation in `docs/`
- Review reference implementation in `firelands-cata-ref/`