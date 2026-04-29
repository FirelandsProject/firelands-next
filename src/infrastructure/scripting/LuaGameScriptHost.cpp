#include <infrastructure/scripting/LuaGameScriptHost.h>

#include <filesystem>
#include <shared/Logger.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Firelands {

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
  return true;
}

void LuaGameScriptHost::Shutdown() {
  if (_L) {
    lua_close(_L);
    _L = nullptr;
  }
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
