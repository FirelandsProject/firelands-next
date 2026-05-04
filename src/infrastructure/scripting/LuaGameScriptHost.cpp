#include <infrastructure/scripting/LuaGameScriptHost.h>

#include <filesystem>
#include <shared/dbc/FactionTemplateDbc.h>
#include <shared/dbc/FactionTemplateHelpers.h>
#include <shared/Logger.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Firelands {

namespace {

int LuaFactionHas(lua_State *L) {
  auto *host = static_cast<LuaGameScriptHost *>(lua_touserdata(L, lua_upvalueindex(1)));
  uint32_t const id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  auto store = host->GetFactionTemplateDbc();
  if (!store || !store->IsLoaded()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  lua_pushboolean(L, store->HasEntry(id) ? 1 : 0);
  return 1;
}

int LuaFactionPrimaryFaction(lua_State *L) {
  auto *host = static_cast<LuaGameScriptHost *>(lua_touserdata(L, lua_upvalueindex(1)));
  uint32_t const id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  auto store = host->GetFactionTemplateDbc();
  if (!store || !store->IsLoaded()) {
    lua_pushinteger(L, 0);
    return 1;
  }
  auto row = store->TryGet(id);
  if (!row) {
    lua_pushinteger(L, 0);
    return 1;
  }
  lua_pushinteger(L, static_cast<lua_Integer>(row->faction));
  return 1;
}

int LuaFactionHostile(lua_State *L) {
  auto *host = static_cast<LuaGameScriptHost *>(lua_touserdata(L, lua_upvalueindex(1)));
  uint32_t const id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  auto store = host->GetFactionTemplateDbc();
  if (!store || !store->IsLoaded()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  auto row = store->TryGet(id);
  if (!row) {
    lua_pushboolean(L, 0);
    return 1;
  }
  lua_pushboolean(L, FactionTemplateLikelyHostileToPlayers(*row) ? 1 : 0);
  return 1;
}

int LuaFactionFriendly(lua_State *L) {
  auto *host = static_cast<LuaGameScriptHost *>(lua_touserdata(L, lua_upvalueindex(1)));
  uint32_t const id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  auto store = host->GetFactionTemplateDbc();
  if (!store || !store->IsLoaded()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  auto row = store->TryGet(id);
  if (!row) {
    lua_pushboolean(L, 0);
    return 1;
  }
  lua_pushboolean(L, FactionTemplateLikelyFriendlyToPlayers(*row) ? 1 : 0);
  return 1;
}

int LuaFactionNeutral(lua_State *L) {
  auto *host = static_cast<LuaGameScriptHost *>(lua_touserdata(L, lua_upvalueindex(1)));
  uint32_t const id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
  auto store = host->GetFactionTemplateDbc();
  if (!store || !store->IsLoaded()) {
    lua_pushboolean(L, 0);
    return 1;
  }
  auto row = store->TryGet(id);
  if (!row) {
    lua_pushboolean(L, 0);
    return 1;
  }
  lua_pushboolean(L, FactionTemplateLikelyNeutralToPlayers(*row) ? 1 : 0);
  return 1;
}

} // namespace

LuaGameScriptHost::LuaGameScriptHost() = default;

LuaGameScriptHost::~LuaGameScriptHost() { Shutdown(); }

bool LuaGameScriptHost::Init(const std::string &scriptsRoot) {
  Shutdown();
  _scriptsRoot = scriptsRoot;

  _L = luaL_newstate();
  if (!_L) {
    LOG_ERROR("luaL_newstate failed");
    return false;
  }

  luaL_openlibs(_L);

  if (!_scriptsRoot.empty()) {
    const std::filesystem::path root(_scriptsRoot);
    const auto boot = root / "bootstrap.lua";
    if (std::filesystem::exists(boot)) {
      const std::string path = boot.string();
      if (luaL_loadfile(_L, path.c_str()) != LUA_OK) {
        const char *msg = lua_tostring(_L, -1);
        LOG_ERROR("Lua bootstrap load failed: {}", msg ? msg : "(no message)");
        lua_pop(_L, 1);
        Shutdown();
        return false;
      }
      if (lua_pcall(_L, 0, 0, 0) != LUA_OK) {
        const char *msg = lua_tostring(_L, -1);
        LOG_ERROR("Lua bootstrap run failed: {}", msg ? msg : "(no message)");
        lua_pop(_L, 1);
        Shutdown();
        return false;
      }
    }
  }
  RegisterFactionLuaApi();
  return true;
}

void LuaGameScriptHost::Shutdown() {
  if (_L) {
    lua_close(_L);
    _L = nullptr;
  }
  _factionTemplateDbc.reset();
}

bool LuaGameScriptHost::RunChunk(const std::string &source,
                                 std::string *errorOut) {
  if (!_L) {
    if (errorOut) {
      *errorOut = "script host not initialized";
    }
    return false;
  }
  if (luaL_loadstring(_L, source.c_str()) != LUA_OK) {
    const char *msg = lua_tostring(_L, -1);
    if (errorOut) {
      *errorOut = msg ? msg : "load error";
    }
    lua_pop(_L, 1);
    return false;
  }
  if (lua_pcall(_L, 0, 0, 0) != LUA_OK) {
    const char *msg = lua_tostring(_L, -1);
    if (errorOut) {
      *errorOut = msg ? msg : "runtime error";
    }
    lua_pop(_L, 1);
    return false;
  }
  return true;
}

void LuaGameScriptHost::FireGossipHello(uint64_t npcGuid) {
  FireEvent("gossip_hello", npcGuid);
}

void LuaGameScriptHost::FireGossipSelect(uint64_t npcGuid, uint32_t menuId,
                                         uint32_t gossipListId) {
  if (!_L) {
    return;
  }
  lua_pushinteger(_L, static_cast<lua_Integer>(menuId));
  lua_setglobal(_L, "_gossipMenuId");
  lua_pushinteger(_L, static_cast<lua_Integer>(gossipListId));
  lua_setglobal(_L, "_gossipListId");
  FireEvent("gossip_select", npcGuid);
}

void LuaGameScriptHost::FireEvent(const std::string &eventName,
                                  uint64_t contextGuid) {
  if (!_L) {
    return;
  }
  lua_getglobal(_L, "OnScriptEvent");
  if (!lua_isfunction(_L, -1)) {
    lua_pop(_L, 1);
    return;
  }
  lua_pushlstring(_L, eventName.data(), eventName.size());
  lua_pushinteger(_L, static_cast<lua_Integer>(contextGuid));
  if (lua_pcall(_L, 2, 0, 0) != LUA_OK) {
    const char *msg = lua_tostring(_L, -1);
    LOG_WARN("OnScriptEvent failed: {}", msg ? msg : "(no message)");
    lua_pop(_L, 1);
  }
}

void LuaGameScriptHost::AttachFactionTemplateDbc(
    std::shared_ptr<FactionTemplateDbc const> store) {
  _factionTemplateDbc = std::move(store);
  RegisterFactionLuaApi();
}

void LuaGameScriptHost::RegisterFactionLuaApi() {
  if (!_L)
    return;
  lua_pushlightuserdata(_L, this);
  lua_pushcclosure(_L, LuaFactionHas, 1);
  lua_setglobal(_L, "firelands_faction_template_has");
  lua_pushlightuserdata(_L, this);
  lua_pushcclosure(_L, LuaFactionPrimaryFaction, 1);
  lua_setglobal(_L, "firelands_faction_template_primary_faction");
  lua_pushlightuserdata(_L, this);
  lua_pushcclosure(_L, LuaFactionHostile, 1);
  lua_setglobal(_L, "firelands_faction_template_hostile_to_players");
  lua_pushlightuserdata(_L, this);
  lua_pushcclosure(_L, LuaFactionFriendly, 1);
  lua_setglobal(_L, "firelands_faction_template_friendly_to_players");
  lua_pushlightuserdata(_L, this);
  lua_pushcclosure(_L, LuaFactionNeutral, 1);
  lua_setglobal(_L, "firelands_faction_template_neutral_to_players");
}

bool LuaGameScriptHost::TryGetGlobalString(const std::string &globalName,
                                           std::string *out) const {
  if (!_L || !out) {
    return false;
  }
  lua_getglobal(_L, globalName.c_str());
  if (!lua_isstring(_L, -1)) {
    lua_pop(_L, 1);
    return false;
  }
  size_t len = 0;
  const char *s = lua_tolstring(_L, -1, &len);
  if (!s) {
    lua_pop(_L, 1);
    return false;
  }
  *out = std::string(s, len);
  lua_pop(_L, 1);
  return true;
}

} // namespace Firelands
