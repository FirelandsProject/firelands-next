# Guía de Scripting LUA

Esta guía explica cómo usar LUA para scripting de gameplay en Firelands.

## Visión General

Firelands usa **Lua 5.4** para scripting de gameplay, permitiendo personalización del lado del servidor sin recompilar el core. Los scripts se cargan desde el directorio configurado (`scripts/lua/` por defecto).

## Configuración

En `worldserver.yaml`:

```yaml
Scripting:
  ScriptsDirectory: "scripts/lua"
```

## Ubicación de Archivos de Script

```
scripts/
└── lua/
    ├── bootstrap.lua      # Opcional: cargado primero para configuración global
    ├── npc_*.lua          # Scripts de NPC
    ├── quest_*.lua        # Scripts de quests
    ├── gameobject_*.lua   # Scripts de GameObject
    └── custom/            # Scripts personalizados
```

## Integración con C++

### LuaGameScriptHost

La clase `LuaGameScriptHost` (en `src/infrastructure/scripting/`) proporciona el puente entre C++ y Lua:

```cpp
class LuaGameScriptHost : public IGameScriptHost {
public:
    bool Init(const std::string& scriptsRoot) override;
    void Shutdown() override;
    void FireEvent(const std::string& eventName, uint64_t contextGuid) override;
    void FireGossipHello(uint64_t npcGuid) override;
    void FireGossipSelect(uint64_t npcGuid, uint32_t menuId, uint32_t gossipListId) override;
    // ...
};
```

### Interfaz de Puerto

El puerto `IGameScriptHost` en `application/ports/IGameScriptHost.h` define el contrato:

```cpp
class IGameScriptHost {
public:
    virtual bool Init(const std::string& scriptsRoot) = 0;
    virtual void Shutdown() = 0;
    virtual void FireEvent(const std::string& eventName, uint64_t contextGuid) = 0;
    virtual void FireGossipHello(uint64_t npcGuid) = 0;
    virtual void FireGossipSelect(uint64_t npcGuid, uint32_t menuId, uint32_t gossipListId) = 0;
    virtual bool TryGetGlobalString(const std::string& globalName, std::string* out) = 0;
};
```

## Sistema de Eventos

### Eventos Disponibles

| Evento | Descripción | Contexto |
|--------|-------------|----------|
| `world_startup` | Llamado cuando el servidor world inicia | Ninguno |
| `world_shutdown` | Llamado cuando el servidor world se detiene | Ninguno |
| `on_spawn` | Llamado cuando criatura/gameobject hace spawn | GUID |
| `on_despawn` | Llamado cuando criatura/gameobject hace despawn | GUID |
| `on_gossip_hello` | Jugador habla con NPC | GUID del NPC |
| `on_gossip_select` | Jugador selecciona opción de gossip | GUID NPC, menu ID, gossip ID |
| `on_enter_combat` | Criatura entra en combate | GUID de criatura |
| `on_death` | Criatura/jugador muere | GUID objetivo |
| `on_killed` | Criatura mata jugador/criatura | GUID killer |
| `on_level_up` | Jugador sube de nivel | GUID del jugador |

### Disparar Eventos desde C++

```cpp
// En código C++ (ej: IA de criatura)
void Creature::OnSpawn() {
    auto script = g_WorldService->GetGameScriptHost();
    if (script) {
        script->FireEvent("on_spawn", GetGuid());
    }
}
```

## Escribiendo Scripts Lua

### Bootstrap

El archivo `bootstrap.lua` se carga primero y puede definir globales:

```lua
-- scripts/lua/bootstrap.lua

-- Funciones helper globales
function GetPlayerName(guid)
    return "Player_" .. guid
end

-- Configuración
CONFIG = {
    debug_mode = false,
    max_level = 85,
}
```

### Scripts de NPC

```lua
-- scripts/lua/npc_12345.lua
-- NPC entry 12345

local npc_events = {}

function npc_events.OnGossipHello(player, npc)
    player:GossipMenuAddItem(0, "Talk to me", 0, 1)
    player:GossipSendMenu(1, npc)
end

function npc_events.OnGossipSelect(player, npc, menu_id, gossip_id)
    if gossip_id == 1 then
        player:SendBroadcastMessage("Hello from Lua!")
        player:GossipComplete()
    end
end

function npc_events.OnEnterCombat(npc, target)
    print("NPC " .. npc .. " entered combat with " .. target)
end

function npc_events.OnDeath(npc, killer)
    print("NPC " .. npc .. " was killed by " .. killer)
    -- Drop custom loot, spawn adds, etc.
end

-- Registrar handlers (para ser llamados desde C++)
return npc_events
```

### Scripts de Quest

```lua
-- scripts/lua/quest_1001.lua
-- Quest ID 1001

local quest_events = {}

function quest_events.OnAccept(player, quest_id)
    print("Player accepted quest " .. quest_id)
    player:AddItem(12345, 1)  -- Add starter item
end

function quest_events.OnComplete(player, quest_id)
    print("Player completed quest " .. quest_id)
    player:AddItem(54321, 1)  -- Reward item
    player:AddExperience(5000)
end

function quest_events.OnProgress(player, quest_id)
    -- Called during quest progress (e.g., kill count updates)
end

return quest_events
```

## API del Objeto Jugador

Cuando Lua se ejecuta en contexto de jugador, estos métodos están disponibles:

| Método | Descripción |
|--------|-------------|
| `player:GetGUID()` | Retorna GUID del jugador |
| `player:GetName()` | Retorna nombre del jugador |
| `player:GetLevel()` | Retorna nivel del jugador |
| `player:AddItem(item_id, count)` | Agrega ítem al jugador |
| `player:RemoveItem(item_id, count)` | Remueve ítem del jugador |
| `player:AddExperience(amount)` | Agrega experiencia |
| `player:AddMoney(copper)` | Agrega dinero |
| `player:LearnSpell(spell_id)` | Enseña hechizo |
| `player:SendBroadcastMessage(text)` | Envía broadcast |
| `player:Teleport(map_id, x, y, z)` | Teletransporta jugador |
| `player:GossipMenuAddItem(icon, text, code, id)` | Agrega opción de gossip |
| `player:GossipSendMenu(npc_text, npc_guid)` | Abre menú de gossip |
| `player:GossipComplete()` | Cierra gossip |

## API del Objeto NPC

| Método | Descripción |
|--------|-------------|
| `npc:GetEntry()` | Retorna entry ID del NPC |
| `npc:GetGUID()` | Retorna GUID del NPC |
| `npc:GetZoneId()` | Retorna zona actual |
| `npc:Say(text, language)` | Hace hablar al NPC |
| `npc:Emote(emote_id)` | Reproduce emote |
| `npc:SpawnCreature(entry, x, y, z, spawntime)` | Spawnea criatura |

## Patrones Comunes

### Menú de Gossip

```lua
local function OnGossipHello(player, npc)
    player:GossipMenuAddItem(0, "I want to trade", 0, 1)
    player:GossipMenuAddItem(0, "Tell me about the area", 0, 2)
    player:GossipMenuAddItem(0, "Goodbye", 0, 100)
    player:GossipSendMenu(1, npc)
end

local function OnGossipSelect(player, npc, menu_id, gossip_id)
    if gossip_id == 1 then
        -- Open shop
        player:SendInventorySlots(1)  -- Example
    elseif gossip_id == 2 then
        player:SendBroadcastMessage("This area is dangerous!")
    elseif gossip_id == 100 then
        player:GossipComplete()
    end
end
```

### Integración de Quest

```lua
local function OnGossipHello(player, npc)
    if player:HasQuest(1001) then
        player:GossipMenuAddItem(0, "Complete Quest", 0, 10)
    end
    if not player:HasQuest(1002) then
        player:GossipMenuAddItem(0, "Accept Quest", 0, 20)
    end
    player:GossipSendMenu(1, npc)
end
```

## Manejo de Errores

### Modo Debug

Habilitar en `bootstrap.lua`:

```lua
CONFIG = {
    debug_mode = true,
}

function debug_print(...)
    if CONFIG and CONFIG.debug_mode then
        print(...)
    end
end
```

### Patrón Try-Catch

```lua
local success, err = pcall(function()
    -- Code that might error
    player:AddItem(12345, 1)
end)

if not success then
    print("Error: " .. err)
end
```

## Mejores Prácticas

1. **Nombrado de archivos**: Usar nombres descriptivos: `npc_9001_boss_karax.lua`
2. **Manejo de errores**: Envolver operaciones riesgosas en `pcall`
3. **Comentarios**: Comentar lógica compleja, especialmente matemáticas/cálculos
4. **Retornar tablas**: Retornar tablas de handlers de eventos para que C++ llame
5. **Evitar globales**: Usar tablas para namespace tus scripts
6. **Pro incrementalmente**: Probar carga de scripts antes de agregar lógica

## Probando Scripts

```bash
# Iniciar servidor world - cargará todos los scripts Lua
./build/bin/world

# Revisar logs por errores de carga de Lua
tail -f logs/firelands-world.log | grep -i lua
```

## Ejemplo: Script Completo de NPC

```lua
-- scripts/lua/npc_9001.lua
-- Innkeeper Johnson - Sanador y giver de quests

local Innkeeper = {}

function Innkeeper.OnGossipHello(player, npc)
    -- Menú principal
    player:GossipMenuAddItem(0, "I need a drink", 0, 1)
    player:GossipMenuAddItem(0, "Tell me about the town", 0, 2)
    player:GossipMenuAddItem(0, "Show me your wares", 0, 3)
    player:GossipMenuAddItem(0, "Goodbye", 0, 100)

    -- Opción de quest
    if player:HasQuest(12345) then
        player:GossipMenuAddItem(3, "About the missing shipment...", 0, 10)
    end

    player:GossipSendMenu(1, npc)
end

function Innkeeper.OnGossipSelect(player, npc, menu_id, gossip_id)
    if gossip_id == 1 then
        player:AddItem(11730, 1)  -- Refreshing Spring Water
        player:GossipComplete()
    elseif gossip_id == 2 then
        player:SendBroadcastMessage("Stormwind is the capital of the Kingdom of Stormwind.")
        player:GossipComplete()
    elseif gossip_id == 3 then
        player:SendInventorySlots(1000)  -- Open vendor
        player:GossipComplete()
    elseif gossip_id == 10 then
        player:SendQuestComplete(12345)
        player:GossipComplete()
    elseif gossip_id == 100 then
        player:GossipComplete()
    end
end

return Innkeeper
```

## Ver También

- [Configuración de Desarrollador](DEVELOPER_SETUP.md)
- [Configuración](worldserver.yaml)
- [Infraestructura](modules/infrastructure.md)
- [Puertos de Aplicación](modules/application.md)