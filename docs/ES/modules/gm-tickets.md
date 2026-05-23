# Sistema de tickets GM (diseño)

Este documento define **persistencia**, **asignación**, **capa de aplicación** y **red (4.3.4 / 15595)** para tickets de ayuda al estilo cliente WoW. Complementa [gm-administration.md](gm-administration.md) (comandos `.` y consola).

## Objetivos

- Un jugador puede **abrir un ticket** desde la UI de ayuda del cliente; el servidor lo **guarda** y responde con los `SMSG_GM_TICKET_*` esperados.
- Un GM (cuenta con permisos adecuados) puede **tomar** tickets de una cola, **responder** y **cerrar**; el jugador ve la respuesta y puede **resolver** (`CMSG_GM_TICKET_RESPONSE_RESOLVE`).
- La cola y el historial viven en **`firelands_characters`** (misma conexión que personajes).

## Persistencia

### Tabla `gm_ticket`

Definida en `sql/18_gm_ticket.sql` y reflejada en `sql/characters_schema.sql` para instalaciones nuevas.

| Columna | Uso |
|--------|-----|
| `id` | Identificador estable del ticket. |
| `account_id` | Cuenta del jugador que abrió el ticket (auth `account.id`). |
| `character_guid` | Personaje asociado; FK a `characters.guid`. |
| `status` | Ver enum `GmTicketStatus` en `domain/models/GmTicket.h`. |
| `category` | Byte de categoría enviado por el cliente (mapear según payload real). |
| `message` | Texto del jugador (validar longitud y caracteres como el chat). |
| `gm_response` | Última respuesta del staff para `SMSG_GMRESPONSE_*` cuando aplique. |
| `map_id`, `pos_*` | Copia de posición al crear (teleporte GM / contexto). |
| `assigned_account_id` | `account.id` del GM asignado, o `NULL` si está en cola abierta. |
| `created_at`, `updated_at`, `assigned_at`, `closed_at` | Auditoría y orden FIFO. |

### Estados (`GmTicketStatus`)

- **Open**: en cola, sin asignar (o asignación pendiente de política).
- **Assigned**: asignado a un `assigned_account_id`.
- **GmAnswered**: el staff envió texto de respuesta; el cliente puede mostrar encuesta / resolver.
- **ClosedResolved**: cerrado por el jugador tras resolver.
- **ClosedAbandoned**: el jugador borró el ticket o abandonó el flujo.
- **ClosedStaff**: cerrado por GM sin pasar por “resolver” del jugador (herramienta interna o comando).

Ajustar la semántica exacta cuando se implementen los handlers para que coincida con lo que el cliente 4.3.4 espera en cada `SMSG_*`.

### Repositorio

- Interfaz: `domain/repositories/IGmTicketRepository.h`.
- Implementación prevista: `MySqlGmTicketRepository` en infrastructure (misma pila JDBC que `MySqlCharacterRepository`).
- **`TryAssign`**: `UPDATE ... SET assigned_account_id = ?, status = 1, assigned_at = NOW() WHERE id = ? AND assigned_account_id IS NULL AND status = 0` y comprobar `affected_rows == 1` para evitar carreras entre dos GMs.

## Asignación y reglas de negocio

1. **Un ticket activo por personaje** (recomendado): antes de `Insert`, comprobar `FindOpenByCharacterGuid` y devolver error al cliente si ya existe.
2. **Cola**: `ListUnassignedOpen(N)` ordenado por `created_at` ASC; los GMs llaman desde consola, comando `.`, o futura UI addon.
3. **Tomar ticket**: servicio de aplicación `GmTicketService::Assign(ticketId, staffAccountId)` que:
   - verifica `AccessLevel` / `Permission` del actor;
   - llama `TryAssign`;
   - opcional: notificar al jugador (`SMSG_GM_TICKET_STATUS_UPDATE` o chat).
4. **Responder**: actualizar `gm_response`, poner `GmAnswered`, emitir **`SMSG_GMRESPONSE_RECEIVED`** con el layout correcto 4.3.4 (ver abajo).
5. **Desconexión del GM**: política configurable (dejar asignado vs volver a Open); documentar en código cuando exista `WorldSession` hook.

## Red (opcodes 15595)

Constantes añadidas en `shared/network/WorldOpcodes.h` (fuente: WowPacketParser `V4_3_4_15595/Opcodes.cs`).

### Cliente → servidor

| Opcode | Valor | Rol |
|--------|-------|-----|
| `CMSG_GM_TICKET_CREATE` | 0x0137 | Crear ticket (mapa, pos, texto, flags, log comprimido opcional). |
| `CMSG_GM_TICKET_UPDATE_TEXT` | 0x0636 | Actualizar mensaje. |
| `CMSG_GM_TICKET_DELETE_TICKET` | 0x6B14 | Borrar / abandonar. |
| `CMSG_GM_TICKET_GET_TICKET` | 0x0326 | Consultar ticket actual (antes no-op en el dispatcher). |
| `CMSG_GM_TICKET_GET_SYSTEM_STATUS` | 0x4205 | Cola habilitada / deshabilitada. |
| `CMSG_GM_TICKET_RESPONSE_RESOLVE` | 0x6506 | Jugador marca resuelto (vacío en muchas builds). |
| `CMSG_GM_SURVEY_SUBMIT` | 0x2724 | Encuesta post-resolución (opcional, fase 2). |

### Servidor → cliente

| Opcode | Valor | Rol |
|--------|-------|-----|
| `SMSG_GM_TICKET_CREATE` | 0x2107 | Resultado de creación (código de respuesta). |
| `SMSG_GM_TICKET_UPDATE_TEXT` | 0x6535 | Resultado de actualización. |
| `SMSG_GM_TICKET_DELETE_TICKET` | 0x6D17 | Confirmación de borrado. |
| `SMSG_GM_TICKET_GET_TICKET` | 0x2C15 | Estado + datos del ticket o “sin ticket”. |
| `SMSG_GM_TICKET_GET_SYSTEM_STATUS` | 0x0D35 | Habilitar/deshabilitar UI. |
| `SMSG_GM_TICKET_STATUS_UPDATE` | 0x2C25 | Cambios de cola / notificación. |
| `SMSG_GMRESPONSE_RECEIVED` | 0x2E34 | Respuesta del GM al cliente. |
| `SMSG_GMRESPONSE_STATUS_UPDATE` | 0x0A04 | Tras resolver (p. ej. mostrar encuesta). |
| `SMSG_GMRESPONSE_DB_ERROR` | 0x0006 | Error de backend (usar con cuidado). |

### Construcción de paquetes

No copiar ciegamente handlers de 3.3.5: Cataclysm cambió parte del soporte. Pasos recomendados:

1. Abrir un sniffer o **WowPacketParser** con un cliente 4.3.4 contra un servidor de referencia que ya tenga tickets, o leer structs generados en el árbol de WowPacketParser para **15595**.
2. Implementar lectura/escritura en `WorldSession` (o un helper `GmTicketPackets.{h,cpp}` en infrastructure) con tests unitarios sobre **capturas hex** fijadas en `tests/data/`.
3. Registrar casos en `WorldSessionDispatcher.cpp` sustituyendo el `break` no-op actual.

## Capa de aplicación (siguiente implementación)

1. **`GmTicketService`** (application): orquesta repositorio + validación + reglas de un ticket por personaje; no conoce sockets.
2. **`WorldSession`**: parsea CMSG, llama al servicio, envía SMSG; comprueba `_accountId` / `_playerGuid` del jugador y `GetAccountAccessLevel()` para rutas de GM.
3. **Permisos**: añadir un bit en `Permissions.h` (p. ej. `ManageGmTickets`) ligado a `GameMaster` por defecto, y comandos `.ticket list`, `.ticket assign`, `.ticket close` que llamen al mismo servicio (consola + juego).
4. **Inyección**: en `world/main.cpp`, crear `MySqlGmTicketRepository(charConn)` y pasar `shared_ptr<GmTicketService>` al factory de `WorldSession` (constructor alargado o struct de dependencias).

## Documentación relacionada

- [gm-administration.md](gm-administration.md) — permisos y comandos existentes.
- [application.md](application.md) — hexagonal y servicios.

## Checklist de implementación

- [x] `MySqlGmTicketRepository` (`sql/18_gm_ticket.sql`, columna `need_more_help` con `Ensure` si la tabla ya existía).
- [x] `GmTicketService` (reglas de negocio + `TryAssign` en SQL).
- [x] Handlers CMSG / SMSG principales (`WorldSessionGmTicketHandlers.cpp`, `GmTicketPackets.cpp`) alineados con WowPacketParser 4.3.x.
- [x] Comandos `.ticket` in-game (`CommandService`, permiso `ManageGmTickets`).
- [ ] Límites / saneamiento de hipervínculos al nivel (opcional).
