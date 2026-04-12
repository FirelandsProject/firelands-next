# Firelands DevTools

The `FirelandsDevTools` utility is a command-line tool designed to help developers manage the Firelands WoW emulator database. It provides shortcuts for common administrative tasks like creating accounts and setting up realms.

## Requirements

- **Compiled Binary**: You must build the project using CMake to generate the `FirelandsDevTools` executable.
- **Running Database**: The tool connects to the MariaDB/MySQL database using the credentials defined in the project configuration (usually the `firelands` user with password `firelands` on localhost).

## Usage

Run the executable from your build's binary directory (usually `build/bin` if following standard CMake patterns):

```bash
./FirelandsDevTools <command> [arguments]
```

### Commands

#### 1. Account Management
Creates or updates a user account in the `auth` database. It handles password hashing and SRP (Secure Remote Password) protocol requirements automatically.

```bash
./FirelandsDevTools account <username> <password> [email] [expansion]
```

- **username**: The name used for login.
- **password**: The plain-text password (hashed by the tool).
- **email**: (Optional) User's email address. Defaults to `<username>@firelands.com`.
- **expansion**: (Optional) Expansion access (0-4). Defaults to `4` (Cataclysm).

**Example:**
```bash
./FirelandsDevTools account admin admin123 admin@example.com 4
```

#### 2. Realm Management
Registers or updates a realm instance in the `realmlist` table.

```bash
./FirelandsDevTools realm <id> <name> <address> <port> [icon] [timezone] [secLevel] [population]
```

- **id**: Unique ID for the realm.
- **name**: Name displayed in the realm list.
- **address**: IP or hostname of the world server.
- **port**: Port of the world server (e.g., `8085`).
- **icon**: (Optional) Realm type icon (0=Normal, 1=PvP, 4=RP, 6=RPPvP, 8=Non-standard). Defaults to `0`.
- **timezone**: (Optional) Realm timezone (1=Development, 2=United States, 3=Oceanic, etc.). Defaults to `1`.
- **secLevel**: (Optional) Minimum security level required to join. Defaults to `0`.
- **population**: (Optional) Population indicator as a float. Defaults to `0.0`.

**Example:**
```bash
./FirelandsDevTools realm 1 "Firelands Test" 127.0.0.1 8085 1 1 0 0.0
```

## Troubleshooting

- **Connection Error**: Ensure the database is running (e.g., via `docker-compose up -d`) and the credentials in `DevTools.cpp` match your environment.
- **Permissions**: Ensure the database user has permissions to write to the `firelands_auth` database.
