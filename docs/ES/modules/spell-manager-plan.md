# Plan: SpellManager (alta carga / rendimiento)

Este documento describe cómo introducir un **SpellManager** en Firelands Next de forma incremental, con **prioridad en rendimiento** para un servidor pensado para **alta concurrencia** (muchos jugadores, muchos casts por tick).

**Dirección de dependencias:** `Infrastructure → Application → Domain → Shared`. El manager vive en **application**; las definiciones de hechizo llegan por **puertos**; DBC/SQL y red quedan en **infrastructure**.

---

## 1. Objetivo y límites

**Objetivo:** Un único componente (`SpellManager` o `SpellCastService`) que centralice:

- ¿Puede el caster lanzar este hechizo en este contexto?
- ¿Qué efectos aplica y qué debe notificarse al mundo/cliente?

**Fuera del alcance inicial (se posponen):** talentos completos, todos los `EffectAura`, reflect, DR completo, vehículos, misiones que otorgan spells, scripts avanzados por spell.

---

## 2. Principios de rendimiento (obligatorios en cada fase)

Estas reglas deben aplicarse **desde la Fase A**, no al final.

### 2.1 Ruta caliente (hot path)

- **Cero o pocas asignaciones heap** en el camino `CMSG_CAST_SPELL` → decisión → `SMSG_*`: preferir buffers reutilizables por hilo o por sesión, `reserve()` en login, evitar `std::string` temporales en logs en niveles distintos de `Debug`.
- **Búsquedas O(1):** definiciones de spell en `unordered_map` / `flat_hash_map` (o vector indexado por id si el rango de ids es denso y se acepta memoria); lista de spells conocidos del jugador como **bitset o `unordered_set`/`flat_hash_set`** según cardinalidad real (perfilad: menos de 200 spells por jugador suele ir bien con set bien hasheado).
- **Datos por valor contiguos:** `SpellDefinition` como `struct` compacta (alineación, sin `std::string` en hot storage; nombres en tabla lateral o solo para GM/logs).
- **Sin locks globales en el cast:** el estado mutable del caster (GCD, cooldowns) debe vivir en un objeto **per sesión** o **por mapa particionado**, no en un mutex único del mundo. Si hace falta compartir, **sharding por `mapId` o por franja de `guid`**.
- **DBC/SQL de definiciones:** cargar en **startup** (o recarga administrativa), no consultar MySQL por cast. Las tablas `spell_dbc` / overrides deben volcarse a **estructuras en memoria** al arrancar o al recargar datos.

### 2.2 Broadcast y red

- **No construir paquetes duplicados** para N observadores si el wire es idéntico: una construcción + broadcast (patrón ya alineado con `Map::BroadcastPacketToNearby`).
- **Batching:** si en el futuro hay muchos `UPDATE_OBJECT` pequeños, agrupar por tick (fuera del alcance del MVP, pero el diseño del manager no debe asumir “un paquete por micro-efecto” sin posibilidad de coalescer).

### 2.3 Validación costosa (LoS, colisión)

- **Cache corta** opcional: resultado LoS caster→objetivo con TTL de unos ms o invalidación por movimiento significativo (evita doble raycast en el mismo tick).
- **Salida temprana:** orden de comprobaciones de más barata a más cara (known spell → definición existe → GCD → cooldown → rango → **al final** LoS).

### 2.4 Multihilo (futuro)

- El diseño del **API del manager** debe permitir que, más adelante, la lógica de cast por mapa ejecute en **un strand/worker por mapa** sin reestructurar todo: hoy puede ser todo en el hilo de networking; mañana, “submit cast job al map queue”.

### 2.5 Observabilidad sin destruir rendimiento

- Contadores atómicos ligeros (casts/s, fallos por razón) en lugar de `spdlog` por cast en `Info`.
- **Muestras** o logs condicionados a rate limit si hace falta depurar en producción.

---

## 3. Arquitectura sugerida

| Pieza | Ubicación | Rol |
|--------|-----------|-----|
| `SpellManager` | `src/application/` | Orquesta validación + efectos + resultado. |
| `SpellCastRequest` / `SpellCastContext` | `src/application/` o `src/domain/` | DTO puro (guids, spellId, targets, snapshot mínimo). |
| `ISpellDefinitionStore` | `src/domain/` (interfaz) | `GetSpell(id)` → `optional<SpellDefinition>`. |
| Implementación DBC+SQL | `src/infrastructure/` | Carga al inicio, mapa en RAM. |
| `ISpellWorldChecks` / colisión | `src/domain/` + impl infra | Rango, LoS (usa `IMapCollisionQueries` cuando aplique). |
| `WorldSession` | `src/infrastructure/` | Parse wire → llama manager → envía salida. |

**Nota sobre paquetes:** o el manager devuelve estructuras “semánticas” y un adaptador en infrastructure construye `WorldPacket`, o (menos puro) el manager usa `SpellCastWire` directamente; para **máximo rendimiento** con tipos estables, la opción intermedia suele ser: **salida = lista de opcodes + payload ya serializado en buffer reutilizable** generado en una capa fina infra.

---

## 4. Fases de implementación

### Fase A — Esqueleto y delegación

**Estado:** implementado (`SpellManager`, `WorldSession` delega, tests en `SpellManagerTests.cpp`). `BuildSpellGo` admite puntero + recuento para evitar `std::vector` en el hot path.

1. Crear `SpellManager` con API clara, p. ej. `TryCast(context, request, output)`.
2. Mover desde `WorldSession::HandleCastSpell`: known spell, GCD fijo actual, generación de `SPELL_START` / `SPELL_GO` / `SPELL_FAILURE` vía `SpellCastWire`.
3. Tests unitarios con dobles: known/unknown, GCD, éxito.

**Rendimiento:** ninguna asignación innecesaria en `TryCast`; `output` pre-reservado por el caller.

### Fase B — Definición de hechizo en memoria

**Estado (parcial):** puerto `ISpellDefinitionStore`, modelo `SpellDefinition`, adaptador `SpellEntryDbcStore` que carga `Spell.dbc` con `SpellEntryfmt` TCPP, valida `fieldCount` + `recordSize`, mapa `unordered_map` O(1). Tras el DBC, `MergeSpellDbcRows(world)` aplica `firelands_world.spell_dbc`: hechizos solo en SQL = fila base + `Ov*`; en hechizos ya en DBC, `PowerType` y columnas `Ov*` no nulas pisan solo esos campos (NULL = heredar DBC). Sin columnas `Ov*` (BD antigua) el merge hace fallback. Integrado en login/GM/`SpellManager`. **Pendiente:** más campos en `SpellDefinition`, validación de rango en mundo, tuning avanzado.

1. Puerto `ISpellDefinitionStore` + `SpellDefinition` compacto.
2. Loader infra: DBC (y opcionalmente merge con `spell_dbc` / `spelleffect_dbc` al arranque).
3. Fallo coherente si no hay definición.

**Rendimiento:** tabla única en RAM; sin SQL en hot path; strings de nombre fuera del struct caliente si no se usan en cast.

### Fase C — Validación de mundo

1. Rango (distancia cheap antes que LoS). **Parcial:** `SpellCastRequest` lleva posiciones mundo opcionales; `SpellManager` usa `ISpellCastTables::GetSpellRangeMinYards` / `GetSpellRangeMaxYards` eligiendo banda hostil vs amistoso (`SpellRange.dbc` índices 0/1): self → amistoso; otro objetivo: delta HP (`SpellEffect` merge) cura/daño; si delta 0, `Spell.dbc` `Attributes` (`SPELL_ATTR0_NEGATIVE_SPELL`, `SPELL_ATTR0_AURA_IS_DEBUFF`) y `AttributesEx` (`SPELL_ATTR_EX_INITIATES_COMBAT`). `SpellDefinition.attributesEx` se carga desde DBC y merge SQL en filas nuevas. Falta paridad total con TCPP `IsPositiveSpell` (efectos/aúras por objetivo). Tolerancias ~3 yd arriba del máximo y ~1.5 yd abajo del mínimo; por debajo del mínimo → `SPELL_FAILED_TOO_CLOSE` (128, referencia cmangos-wotlk / verificar UI 15595).
2. LoS / colisión cuando exista datos; flag para desactivar en pruebas.

**Rendimiento:** orden estricto cheap→caro; caché opcional de LoS documentada.

### Fase D — Primer efecto jugable

1. Pipeline de efectos extensible (`IEffectHandler` o tabla de punteros a funciones).
2. Un solo tipo: daño o curación directa a unidad con update de vida vía update fields existentes.
3. Sin persistir cada tick de DoT en esta fase.

**Estado (pipeline):** composición de golpe en `application/spell/SpellHitEffects.*` (`ResolvePrimarySpellHitUnitGuid`, `ApplyImmediateHealthFromDefinition`, …). `SpellManager` solo valida y delega ahí para no mezclar reglas de targeting/efecto con GCD/cooldowns/red.

**Estado:** `SpellDefinition.immediateHealthEffectDelta` se rellena desde cliente `SpellEffect.dbc` (efectos 2 daño por escuela, 10 curación): primera fila por `SpellID` en orden de `EffectIndex`; magnitud aproximada `(EffectBasePoints + 1) + punto medio del dado` cuando `EffectDieSides > 0`. Daño → delta negativo; curación → positivo. Sin columnas MVP en `spell_dbc` (limpieza migración `22_world_spell_dbc_drop_mvp_columns.sql`). Tras un cast válido, `SpellCastOutcome` indica objetivo y delta; `WorldSession` aplica en `Player` y emite `SMSG_UPDATE_OBJECT` (`UPDATETYPE_VALUES`) vía `BuildPlayerHealthValuesUpdate`. Criaturas / persistencia de vida en logout: pendiente.

**Rendimiento:** evitar virtual calls profundos en cadena caliente si el profiler lo pide: switch por “family” de efecto o tabla estática por `Effect` id acotado.

### Fase E — GCD real, cooldowns, poder

1. Estado por caster: estructuras fijas o `small_vector` por tipo de recurso.
2. Datos desde `SpellDefinition` + stats (snapshot en contexto para no bloquear).

**Rendimiento:** no escanear listas largas; cooldown como mapa plano por spellId por sesión.

**Estado:** `Spell.dbc` carga `cooldownsId` → `SpellCooldowns.dbc` vía `ISpellCastTables::GetCooldownTiming` (`SpellCastTablesDbc` carga `SpellCooldowns.dbc`). GCD del cast = `StartRecoveryTime` si > 0, si no 1500 ms. `RecoveryTime` del mismo row se publica en `SpellCastOutcome.spellCooldownDurationMs`; `WorldSession` guarda `_spellCooldownUntil[spellId]`. Maná: `SpellDefinition.manaCost` desde `SpellPower.dbc` (`spellPowerId` en `Spell.dbc`) tras `SpellEntryDbcStore::ApplySpellPowerManaFromTables`; snapshot `casterPower1` desde `Player`; fallo `SPELL_FAILED_NO_POWER`; en éxito `power1Delta` + `SMSG_UPDATE_OBJECT` POWER1. Escalado por stats / otros power types: pendiente.

### Fase F — Auras, procs, scripts

1. Consumir tablas `spell_proc`, `spell_linked_spell`, etc., cargadas al inicio.
2. Auras: store por unidad, reglas de stack desde `spell_group*`.
3. Hooks Lua en eventos acotados (no por cada sub-tick si se puede agrupar).

**Rendimiento:** auras en estructura por unidad con inserción/eliminación O(k) con k pequeño; evitar barridos globales del mundo por cada aura tick (iterar solo unidades “con auras activas”).

---

## 5. Entregables por PR (sugerido)

1. **PR1:** Fase A + tests + notas de rendimiento en código (comentarios breves donde se reserve memoria).
2. **PR2:** Fase B (store + definición mínima DBC).
3. **PR3:** Fase C (rango + LoS).
4. **PR4:** Fase D (un efecto + vida).
5. **PR5+:** Fases E y F según prioridad.

---

## 6. Criterios de aceptación globales

- Ningún **SELECT** a MySQL en el camino de cast estable (salvo herramientas GM explícitas).
- Complejidad esperada del cast feliz: **O(1)** respecto al número de jugadores del reino; respecto a observadores cercanos, lineal solo en radio (aceptable) y optimizable con grid del mapa.
- Perfilado con build `Release` antes de declarar “listo” cada fase que toque hot path.

---

## 7. Referencias internas

- Cast actual: `WorldSession::HandleCastSpell`, `SpellCastWire`.
- Definición de hechizos: `ISpellDefinitionStore` + `SpellEntryDbcStore` (`Spell.dbc`); dificultad: `SpellDifficultyDbc`.
- Tablas world recientes: migración `16_world_spell_tables.sql` (paridad esquema con TCPP para datos/overrides).

---

*Última actualización: plan inicial acordado en chat; revisar tras cada fase según métricas reales (CPU/tick, latencia de red, contención).*
