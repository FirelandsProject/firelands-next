# Database Schema Guide

This document describes the Firelands database schema, including all tables, their purposes, and how to work with them as a developer.

## Overview

Firelands uses three logical databases:

| Database | Purpose |
|----------|---------|
| `firelands_auth` | Authentication, accounts, realm list |
| `firelands_characters` | Player characters, items, tickets |
| `firelands_world` | Static game data (NPCs, quests, items, etc.) |

## Schema Files Location

```
sql/
├── init/              # Base schema (run once)
│   ├── auth_schema.sql
│   ├── characters_schema.sql
│   └── world_schema.sql
├── migrations/        # Incremental changes (ordered by prefix)
│   └── *.sql
└── bundled/          # Merged schema for Docker deployment
    ├── firelands_auth.sql
    ├── firelands_characters.sql
    └── firelands_world.sql
```

## Database Authentication (`firelands_auth`)

### `account` Table

Stores user account information. Uses SRP-6a for secure authentication.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INT (PK) | Auto-increment account ID |
| `username` | VARCHAR(32) | Unique username (case-sensitive) |
| `salt` | BINARY(32) | SRP salt |
| `verifier` | BINARY(32) | SRP verifier |
| `email` | VARCHAR(255) | Account email |
| `joindate` | TIMESTAMP | Account creation date |
| `last_ip` | VARCHAR(15) | Last login IP |
| `expansion` | TINYINT | Expansion level (3 = Cataclysm) |
| `locked` | TINYINT | Account lock (1 = banned) |
| `access_level` | TINYINT | GM level (0-3) |

**Developer Notes:**
- Password is never stored; only salt and verifier
- Use `SRPService` in C++ for authentication flow
- The `.account` console commands interact with this table
- See `MySqlAccountRepository` in `src/infrastructure/persistence/`

### `realmlist` Table

Defines available game realms displayed in the client realm list.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INT (PK) | Realm ID |
| `name` | VARCHAR(32) | Realm display name |
| `address` | VARCHAR(255) | World server address |
| `port` | SMALLINT | World server port |
| `icon` | TINYINT | Realm type icon (0=Normal, 1=PvP, etc.) |
| `timezone` | TINYINT | Realm timezone |
| `allowedSecurityLevel` | TINYINT | Minimum access level |
| `population` | FLOAT | Population level |

### `account_session` Table

Stores active session keys for logged-in accounts.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INT (PK, FK) | References `account.id` |
| `session_key` | BINARY(40) | Session key for world server |

### `account_data` Table

Stores cached client UI data (macros, keybinds, etc.).

| Column | Type | Description |
|--------|------|-------------|
| `accountId` | INT (PK) | Account ID |
| `type` | TINYINT (PK) | Data type (0-8) |
| `time` | INT | Last modification time |
| `data` | BLOB | Serialized data |

---

## Database Characters (`firelands_characters`)

### `characters` Table

Main player character table.

| Column | Type | Description |
|--------|------|-------------|
| `guid` | INT (PK) | Character global unique ID |
| `account` | INT | Owner account ID |
| `name` | VARCHAR(12) | Character name |
| `race` | TINYINT | Race ID |
| `class` | TINYINT | Class ID |
| `gender` | TINYINT | Gender (0=Male, 1=Female) |
| `skin`, `face`, `hairStyle`, `hairColor`, `facialHair` | TINYINT | Appearance customization |
| `outfitId` | TINYINT | Predefined outfit (CharStartOutfit.dbc) |
| `equipmentCache` | MEDIUMTEXT | Cached equipment (JSON) |
| `level` | TINYINT | Character level (1-85) |
| `zoneId` | SMALLINT | Current zone |
| `mapId` | SMALLINT | Current map |
| `x`, `y`, `z` | FLOAT | Position |
| `orientation` | FLOAT | Facing direction |
| `guildId` | INT | Guild membership |
| `money` | INT | Copper amount |
| `xp` | INT | Current XP |
| `live_health` | INT (NULL) | Current health (runtime) |
| `live_power1` | INT (NULL) | Current power (runtime) |
| `tutorial0-7` | INT | Tutorial flags |

**Developer Notes:**
- Use `MySqlCharacterRepository` in infrastructure layer
- Character creation flows through `CharacterService` and `PlayerCreateInfoService`
- Equipment cache is JSON-serialized equipment data
- Position/map updates on world entry and movement

### `character_spell` Table

Stores spells learned by characters (for `.learn` GM command).

| Column | Type | Description |
|--------|------|-------------|
| `guid` | INT (PK, FK) | Character GUID |
| `spell` | INT (PK) | Spell ID |

### `gm_ticket` Table

GM help ticket system.

| Column | Type | Description |
|--------|------|-------------|
| `id` | BIGINT (PK) | Ticket ID |
| `account_id` | INT | Owner account |
| `character_guid` | INT | Owner character |
| `status` | TINYINT | 0=Open, 1=Answered, 2=Closed |
| `category` | TINYINT | Ticket category |
| `need_more_help` | TINYINT | Flag for escalation |
| `message` | TEXT | Initial ticket message |
| `gm_response` | TEXT | GM response |
| `map_id`, `pos_x`, `pos_y`, `pos_z` | - | Player position when created |
| `assigned_account_id` | INT | Assigned GM |
| `created_at`, `updated_at` | TIMESTAMP | Timestamps |
| `assigned_at`, `closed_at` | TIMESTAMP | Assignment/close times |

**Developer Notes:**
- See `GmTicketService` in application layer
- Handler in `WorldSessionGmTicketHandlers.cpp`
- Wire format follows 4.3.4 protocol (`CMSG_GM_TICKET_*`)

### `mail` Table

In-game mail system.

| Column | Type | Description |
|--------|------|-------------|
| `id` | BIGINT (PK) | Mail ID |
| `receiver_guid` | INT | Recipient character |
| `sender_guid` | INT | Sender character (0 = system) |
| `subject` | VARCHAR(200) | Mail subject |
| `body` | TEXT | Mail body |
| `deliver_time` | INT | Delivery timestamp |
| `expire_time` | INT | Expiration timestamp |
| `checked` | TINYINT | Read flag |

### `mail_items` Table

Items attached to mail.

| Column | Type | Description |
|--------|------|-------------|
| `mail_id` | BIGINT (PK, FK) | Mail reference |
| `item_guid` | INT (PK) | Item GUID |
| `receiver_guid` | INT (FK) | Item owner |

### Instance Tables

Instance and raid persistence:

| Table | Purpose |
|-------|---------|
| `instance` | Instance save data |
| `instance_reset` | Instance reset times |
| `character_instance` | Character's instance bindings |
| `group_instance` | Group instance bindings |
| `account_instance_times` | Account instance time limits |
| `item_refund_instance` | Item refund tracking |

### `character_account_data` Table

Per-character UI data (distinct from account-wide).

| Column | Type | Description |
|--------|------|-------------|
| `guid` | INT (PK, FK) | Character GUID |
| `type` | TINYINT (PK) | Data type |
| `time` | INT | Last update |
| `data` | BLOB | Serialized data |

---

## Database World (`firelands_world`)

### `version` Table

Tracks schema version.

| Column | Type | Description |
|--------|------|-------------|
| `core_version` | VARCHAR(120) | Firelands core version |
| `db_version` | VARCHAR(120) | Database version |

**Note:** Most static game data (NPCs, quests, items, etc.) comes from client DBC files, not the database. The world DB typically contains:
- Creature spawns
- GameObject spawns
- Quest definitions
- Item templates
- Custom server-side data

---

## Working with Migrations

### Creating a New Migration

1. Create file in `sql/migrations/`:
   ```bash
   touch sql/migrations/001_my_feature.sql
   ```

2. Write SQL with idempotent operations:
   ```sql
   ALTER TABLE account ADD COLUMN new_column INT DEFAULT 0;
   ```

3. The `DatabaseMigrator` runs files in lexicographic order and tracks applied migrations in `schema_migrations` table.

### Migration Best Practices

- **Always use** `IF NOT EXISTS`, `ADD COLUMN IF NOT EXISTS`, etc.
- **Never** use `DROP TABLE` in migrations (may lose data)
- **Use** `ALTER TABLE` for schema changes
- **Prefix** migration files with numbers: `001_`, `002_`, etc.
- **Test** migrations on a copy of production data before deploying

### Regenerating Bundled SQL

After adding migrations, regenerate bundled SQL:
```bash
cmake --build build --target merge-migrations
```

Or manually:
```bash
python3 tools/merge_migrations.py
```

---

## C++ Integration

### Repository Pattern

All database access goes through repository interfaces in the **domain** layer:

```cpp
// Domain interface (port)
class ICharacterRepository {
    virtual std::optional<Character> FindByGuid(uint32 guid) = 0;
    virtual void Save(const Character& character) = 0;
    // ...
};

// Infrastructure implementation
class MySqlCharacterRepository : public ICharacterRepository {
    // Uses MariaDB C++ connector
};
```

### Key Repository Classes

| Interface | Implementation | Database |
|-----------|---------------|----------|
| `IAccountRepository` | `MySqlAccountRepository` | auth |
| `IRealmRepository` | `MySqlRealmRepository` | auth |
| `ICharacterRepository` | `MySqlCharacterRepository` | characters |
| `IPlayerCreateInfoRepository` | `MySqlPlayerCreateInfoRepository` | world |
| `IGmTicketRepository` | `MySqlGmTicketRepository` | characters |
| `ICreatureSpawnRepository` | `MySqlCreatureSpawnRepository` | world |

### Service Layer

Services in **application** layer use repositories:

```cpp
class CharacterService {
    std::vector<Character> GetCharactersForAccount(int accountId);
    // Uses ICharacterRepository (injected)
};
```

---

## Common Development Tasks

### Adding a New Table

1. Add schema to `sql/init/` or a migration in `sql/migrations/`
2. Create repository interface in `src/domain/repositories/`
3. Implement in `src/infrastructure/persistence/MySql*.cpp`
4. Register in service or use directly

### Modifying Existing Tables

1. Create migration in `sql/migrations/`
2. Update C++ model if needed
3. Update repository if needed

### Querying the Database

```bash
# Using MySQL CLI
mysql -u firelands -p firelands_auth

# Using Docker
docker exec -it firelands-db-1 mysql -u firelands -p
```

---

## See Also

- [Developer Setup](DEVELOPER_SETUP.md)
- [Application Layer](modules/application.md)
- [Infrastructure](modules/infrastructure.md)
- [GM Administration](modules/gm-administration.md)