# LUA Scripting Guide

This guide explains how to use LUA for gameplay scripting in Firelands.

## Overview

Firelands uses **Lua 5.4** for gameplay scripting, allowing server-side customization without rebuilding the core. Scripts are loaded from the configured scripts directory (`scripts/lua/` by default).

## Configuration

In `worldserver.yaml`:

```yaml
Scripting:
  ScriptsDirectory: "scripts/lua"
```

## Script Files Location

```
scripts/
└── lua/
    ├── bootstrap.lua      # Optional: loaded first for global setup
    ├── npc_*.lua          # NPC scripts
    ├── quest_*.lua        # Quest scripts
    ├── gameobject_*.lua   # GameObject scripts
    └── custom/            # Custom scripts
```

## C++ Integration

### LuaGameScriptHost

The `LuaGameScriptHost` class (in `src/infrastructure/scripting/`) provides the bridge between C++ and Lua:

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

### Port Interface

The `IGameScriptHost` port in `application/ports/IGameScriptHost.h` defines the contract:

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

## Event System

### Available Events

| Event | Description | Context |
|-------|-------------|---------|
| `world_startup` | Called when world server starts | None |
| `world_shutdown` | Called when world server stops | None |
| `on_spawn` | Called when creature/gameobject spawns | GUID |
| `on_despawn` | Called when creature/gameobject despawns | GUID |
| `on_gossip_hello` | Player talks to NPC | NPC GUID |
| `on_gossip_select` | Player selects gossip option | NPC GUID, menu ID, gossip ID |
| `on_enter_combat` | Creature enters combat | Creature GUID |
| `on_death` | Creature/player dies | Target GUID |
| `on_killed` | Creature kills player/creature | Killer GUID |
| `on_level_up` | Player levels up | Player GUID |

### Firing Events from C++

```cpp
// In C++ code (e.g., creature AI)
void Creature::OnSpawn() {
    auto script = g_WorldService->GetGameScriptHost();
    if (script) {
        script->FireEvent("on_spawn", GetGuid());
    }
}
```

## Writing Lua Scripts

### Bootstrap

The `bootstrap.lua` file is loaded first and can define globals:

```lua
-- scripts/lua/bootstrap.lua

-- Global helper functions
function GetPlayerName(guid)
    return "Player_" .. guid
end

-- Configuration
CONFIG = {
    debug_mode = false,
    max_level = 85,
}
```

### NPC Scripts

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

-- Register handlers (to be called from C++)
return npc_events
```

### Quest Scripts

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

## Player Object API

When Lua is executed in player context, these methods are available:

| Method | Description |
|--------|-------------|
| `player:GetGUID()` | Returns player GUID |
| `player:GetName()` | Returns player name |
| `player:GetLevel()` | Returns player level |
| `player:AddItem(item_id, count)` | Adds item to player |
| `player:RemoveItem(item_id, count)` | Removes item from player |
| `player:AddExperience(amount)` | Adds experience |
| `player:AddMoney(copper)` | Adds money |
| `player:LearnSpell(spell_id)` | Teaches spell |
| `player:SendBroadcastMessage(text)` | Sends broadcast |
| `player:Teleport(map_id, x, y, z)` | Teleports player |
| `player:GossipMenuAddItem(icon, text, code, id)` | Adds gossip option |
| `player:GossipSendMenu(npc_text, npc_guid)` | Opens gossip menu |
| `player:GossipComplete()` | Closes gossip |

## NPC Object API

| Method | Description |
|--------|-------------|
| `npc:GetEntry()` | Returns NPC entry ID |
| `npc:GetGUID()` | Returns NPC GUID |
| `npc:GetZoneId()` | Returns current zone |
| `npc:Say(text, language)` | Makes NPC speak |
| `npc:Emote(emote_id)` | Plays emote |
| `npc:SpawnCreature(entry, x, y, z, spawntime)` | Spawns creature |

## Common Patterns

### Gossip Menu

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

### Quest Integration

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

## Error Handling

### Debug Mode

Enable in `bootstrap.lua`:

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

### Try-Catch Pattern

```lua
local success, err = pcall(function()
    -- Code that might error
    player:AddItem(12345, 1)
end)

if not success then
    print("Error: " .. err)
end
```

## Best Practices

1. **File naming**: Use descriptive names: `npc_9001_boss_karax.lua`
2. **Error handling**: Wrap risky operations in `pcall`
3. **Comments**: Comment complex logic, especially math/calculations
4. **Return tables**: Return event handler tables for C++ to call
5. **Avoid globals**: Use tables to namespace your scripts
6. **Test incrementally**: Test script loading before adding logic

## Testing Scripts

```bash
# Start world server - it will load all Lua scripts
./build/bin/world

# Check logs for Lua loading errors
tail -f logs/firelands-world.log | grep -i lua
```

## Example: Complete NPC Script

```lua
-- scripts/lua/npc_9001.lua
-- Innkeeper Johnson - Healer and Quest giver

local Innkeeper = {}

function Innkeeper.OnGossipHello(player, npc)
    -- Main menu
    player:GossipMenuAddItem(0, "I need a drink", 0, 1)
    player:GossipMenuAddItem(0, "Tell me about the town", 0, 2)
    player:GossipMenuAddItem(0, "Show me your wares", 0, 3)
    player:GossipMenuAddItem(0, "Goodbye", 0, 100)

    -- Quest option
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

## See Also

- [Developer Setup](DEVELOPER_SETUP.md)
- [Configuration](worldserver.yaml)
- [Infrastructure](modules/infrastructure.md)
- [Application Ports](modules/application.md)