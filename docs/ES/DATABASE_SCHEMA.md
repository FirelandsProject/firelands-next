# Guía del Esquema de Base de Datos

Este documento describe el esquema de base de datos de Firelands, incluyendo todas las tablas, sus propósitos y cómo trabajar con ellas como desarrollador.

## Visión General

Firelands usa tres bases de datos lógicas:

| Base de Datos | Propósito |
|---------------|-----------|
| `firelands_auth` | Autenticación, cuentas, lista de reinos |
| `firelands_characters` | Personajes de jugadores, ítems, tickets |
| `firelands_world` | Datos estáticos del juego (NPCs, quests, ítems, etc.) |

## Ubicación de Archivos de Esquema

```
sql/
├── init/              # Esquema base (ejecutar una vez)
│   ├── auth_schema.sql
│   ├── characters_schema.sql
│   └── world_schema.sql
├── migrations/        # Cambios incrementales (ordenados por prefijo)
│   └── *.sql
└── bundled/          # Esquema combinado para Docker
    ├── firelands_auth.sql
    ├── firelands_characters.sql
    └── firelands_world.sql
```

## Base de Datos de Autenticación (`firelands_auth`)

### Tabla `account`

Almacena información de cuentas de usuario. Usa SRP-6a para autenticación segura.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `id` | INT (PK) | ID de cuenta auto-incremental |
| `username` | VARCHAR(32) | Nombre de usuario único |
| `salt` | BINARY(32) | Salt SRP |
| `verifier` | BINARY(32) | Verificador SRP |
| `email` | VARCHAR(255) | Email de la cuenta |
| `joindate` | TIMESTAMP | Fecha de creación |
| `last_ip` | VARCHAR(15) | Último IP de login |
| `expansion` | TINYINT | Nivel de expansión (3 = Cataclysm) |
| `locked` | TINYINT | Bloqueo de cuenta (1 = baneado) |
| `access_level` | TINYINT | Nivel GM (0-3) |

**Notas para Desarrolladores:**
- La contraseña nunca se almacena; solo salt y verifier
- Usa `SRPService` en C++ para el flujo de autenticación
- Los comandos de consola `.account` interactúan con esta tabla
- Ver `MySqlAccountRepository` en `src/infrastructure/persistence/`

### Tabla `realmlist`

Define los reinos disponibles mostrados en la lista de reinos del cliente.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `id` | INT (PK) | ID del reino |
| `name` | VARCHAR(32) | Nombre para mostrar |
| `address` | VARCHAR(255) | Dirección del servidor world |
| `port` | SMALLINT | Puerto del servidor world |
| `icon` | TINYINT | Tipo de icono del reino (0=Normal, 1=PvP, etc.) |
| `timezone` | TINYINT | Zona horaria del reino |
| `allowedSecurityLevel` | TINYINT | Nivel mínimo de acceso |
| `population` | FLOAT | Nivel de población |

### Tabla `account_session`

Almacena claves de sesión activas para cuentas logueadas.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `id` | INT (PK, FK) | Referencia a `account.id` |
| `session_key` | BINARY(40) | Clave de sesión para el servidor world |

### Tabla `account_data`

Almacena datos de UI del cliente en caché (macros, keybinds, etc.).

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `accountId` | INT (PK) | ID de cuenta |
| `type` | TINYINT (PK) | Tipo de dato (0-8) |
| `time` | INT | Tiempo de última modificación |
| `data` | BLOB | Datos serializados |

---

## Base de Datos de Personajes (`firelands_characters`)

### Tabla `characters`

Tabla principal de personajes de jugadores.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `guid` | INT (PK) | ID global único del personaje |
| `account` | INT | ID de cuenta propietaria |
| `name` | VARCHAR(12) | Nombre del personaje |
| `race` | TINYINT | ID de raza |
| `class` | TINYINT | ID de clase |
| `gender` | TINYINT | Género (0=Masculino, 1=Femenino) |
| `skin`, `face`, `hairStyle`, `hairColor`, `facialHair` | TINYINT | Personalización de apariencia |
| `outfitId` | TINYINT | Outfit predefinido (CharStartOutfit.dbc) |
| `equipmentCache` | MEDIUMTEXT | Equipo en caché (JSON) |
| `level` | TINYINT | Nivel del personaje (1-85) |
| `zoneId` | SMALLINT | Zona actual |
| `mapId` | SMALLINT | Mapa actual |
| `x`, `y`, `z` | FLOAT | Posición |
| `orientation` | FLOAT | Dirección |
| `guildId` | INT | Membresía de hermandad |
| `money` | INT | Cantidad de cobre |
| `xp` | INT | XP actual |
| `live_health` | INT (NULL) | Salud actual (runtime) |
| `live_power1` | INT (NULL) | Poder actual (runtime) |
| `tutorial0-7` | INT | Banderas de tutorial |

**Notas para Desarrolladores:**
- Usa `MySqlCharacterRepository` en la capa de infraestructura
- La creación de personajes pasa por `CharacterService` y `PlayerCreateInfoService`
- El caché de equipo es equipo serializado en JSON
- Actualizaciones de posición/mapa en entrada al mundo y movimiento

### Tabla `character_spell`

Almacena hechizos aprendidos por personajes (para comando GM `.learn`).

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `guid` | INT (PK, FK) | GUID del personaje |
| `spell` | INT (PK) | ID del hechizo |

### Tabla `gm_ticket`

Sistema de tickets de ayuda de GM.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `id` | BIGINT (PK) | ID del ticket |
| `account_id` | INT | Cuenta propietaria |
| `character_guid` | INT | Personaje propietario |
| `status` | TINYINT | 0=Abierto, 1=Respondido, 2=Cerrado |
| `category` | TINYINT | Categoría del ticket |
| `need_more_help` | TINYINT | Bandera de escalación |
| `message` | TEXT | Mensaje inicial del ticket |
| `gm_response` | TEXT | Respuesta del GM |
| `map_id`, `pos_x`, `pos_y`, `pos_z` | - | Posición del jugador al crear |
| `assigned_account_id` | INT | GM asignado |
| `created_at`, `updated_at` | TIMESTAMP | Marcas de tiempo |
| `assigned_at`, `closed_at` | TIMESTAMP | Tiempos de asignación/cierre |

**Notas para Desarrolladores:**
- Ver `GmTicketService` en la capa de aplicación
- Handler en `WorldSessionGmTicketHandlers.cpp`
- El formato de red sigue el protocolo 4.3.4 (`CMSG_GM_TICKET_*`)

### Tabla `mail`

Sistema de correo en el juego.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `id` | BIGINT (PK) | ID del correo |
| `receiver_guid` | INT | Personaje receptor |
| `sender_guid` | INT | Personaje emisor (0 = sistema) |
| `subject` | VARCHAR(200) | Asunto del correo |
| `body` | TEXT | Cuerpo del correo |
| `deliver_time` | INT | Tiempo de entrega |
| `expire_time` | INT | Tiempo de expiración |
| `checked` | TINYINT | Bandera de leído |

### Tabla `mail_items`

Ítems adjuntos al correo.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `mail_id` | BIGINT (PK, FK) | Referencia al correo |
| `item_guid` | INT (PK) | GUID del ítem |
| `receiver_guid` | INT (FK) | Propietario del ítem |

### Tablas de Instancias

Persistencia de instancias y raids:

| Tabla | Propósito |
|-------|-----------|
| `instance` | Datos guardados de instancia |
| `instance_reset` | Tiempos de reseteo de instancia |
| `character_instance` | Vinculaciones de instancia del personaje |
| `group_instance` | Vinculaciones de instancia de grupo |
| `account_instance_times` | Límites de tiempo de instancia de cuenta |
| `item_refund_instance` | Seguimiento de reembolso de ítems |

### Tabla `character_account_data`

Datos de UI por personaje (distinto de la cuenta).

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `guid` | INT (PK, FK) | GUID del personaje |
| `type` | TINYINT (PK) | Tipo de dato |
| `time` | INT | Última actualización |
| `data` | BLOB | Datos serializados |

---

## Base de Datos del Mundo (`firelands_world`)

### Tabla `version`

Rastreo de versión del esquema.

| Columna | Tipo | Descripción |
|---------|------|-------------|
| `core_version` | VARCHAR(120) | Versión del core Firelands |
| `db_version` | VARCHAR(120) | Versión de la base de datos |

**Nota:** La mayoría de los datos estáticos del juego (NPCs, quests, ítems, etc.) vienen de archivos DBC del cliente, no de la base de datos. La DB del mundo típicamente contiene:
- Spawns de criaturas
- Spawns de GameObjects
- Definiciones de quests
- Plantillas de ítems
- Datos personalizados del servidor

---

## Trabajando con Migraciones

### Crear una Nueva Migración

1. Crear archivo en `sql/migrations/`:
   ```bash
   touch sql/migrations/001_mi_caracteristica.sql
   ```

2. Escribir SQL con operaciones idempotentes:
   ```sql
   ALTER TABLE account ADD COLUMN nueva_columna INT DEFAULT 0;
   ```

3. El `DatabaseMigrator` ejecuta los archivos en orden lexicográfico y rastrea las migraciones aplicadas en la tabla `schema_migrations`.

### Mejores Prácticas de Migraciones

- **Siempre usar** `IF NOT EXISTS`, `ADD COLUMN IF NOT EXISTS`, etc.
- **Nunca usar** `DROP TABLE` en migraciones (puede perder datos)
- **Usar** `ALTER TABLE` para cambios de esquema
- **Prefijar** archivos de migración con números: `001_`, `002_`, etc.
- **Probar** migraciones en una copia de datos de producción antes de desplegar

### Regenerar SQL Bundled

Después de agregar migraciones, regenerar SQL bundled:
```bash
cmake --build build --target merge-migrations
```

O manualmente:
```bash
python3 tools/merge_migrations.py
```

---

## Integración con C++

### Patrón de Repositorio

Todo acceso a la base de datos pasa por interfaces de repositorio en la capa **domain**:

```cpp
// Interfaz de dominio (puerto)
class ICharacterRepository {
    virtual std::optional<Character> FindByGuid(uint32 guid) = 0;
    virtual void Save(const Character& character) = 0;
    // ...
};

// Implementación de infraestructura
class MySqlCharacterRepository : public ICharacterRepository {
    // Usa MariaDB C++ connector
};
```

### Clases de Repositorio Clave

| Interfaz | Implementación | Base de Datos |
|----------|---------------|---------------|
| `IAccountRepository` | `MySqlAccountRepository` | auth |
| `IRealmRepository` | `MySqlRealmRepository` | auth |
| `ICharacterRepository` | `MySqlCharacterRepository` | characters |
| `IPlayerCreateInfoRepository` | `MySqlPlayerCreateInfoRepository` | world |
| `IGmTicketRepository` | `MySqlGmTicketRepository` | characters |
| `ICreatureSpawnRepository` | `MySqlCreatureSpawnRepository` | world |

### Capa de Servicios

Los servicios en la capa **application** usan repositorios:

```cpp
class CharacterService {
    std::vector<Character> GetCharactersForAccount(int accountId);
    // Usa ICharacterRepository (inyectado)
};
```

---

## Tareas Comunes de Desarrollo

### Agregar una Nueva Tabla

1. Agregar esquema a `sql/init/` o una migración en `sql/migrations/`
2. Crear interfaz de repositorio en `src/domain/repositories/`
3. Implementar en `src/infrastructure/persistence/MySql*.cpp`
4. Registrar en servicio o usar directamente

### Modificar Tablas Existentes

1. Crear migración en `sql/migrations/`
2. Actualizar modelo C++ si es necesario
3. Actualizar repositorio si es necesario

### Consultar la Base de Datos

```bash
# Usando MySQL CLI
mysql -u firelands -p firelands_auth

# Usando Docker
docker exec -it firelands-db-1 mysql -u firelands -p
```

---

## Ver También

- [Configuración de Desarrollador](DEVELOPER_SETUP.md)
- [Capa de Aplicación](modules/application.md)
- [Infraestructura](modules/infrastructure.md)
- [Administración GM](modules/gm-administration.md)