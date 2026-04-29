#ifndef FIRELANDS_INFRASTRUCTURE_SCRIPTING_LUA_GAME_SCRIPT_HOST_H
#define FIRELANDS_INFRASTRUCTURE_SCRIPTING_LUA_GAME_SCRIPT_HOST_H

#include <application/ports/IGameScriptHost.h>
#include <string>

struct lua_State;

namespace Firelands {

class LuaGameScriptHost final : public IGameScriptHost {
public:
  LuaGameScriptHost();
  ~LuaGameScriptHost() override;

  LuaGameScriptHost(const LuaGameScriptHost &) = delete;
  LuaGameScriptHost &operator=(const LuaGameScriptHost &) = delete;

  bool Init(const std::string &scriptsRoot) override;
  void Shutdown() override;
  bool RunChunk(const std::string &source, std::string *errorOut) override;
  void FireEvent(const std::string &eventName, uint64_t contextGuid) override;
  bool TryGetGlobalString(const std::string &globalName,
                          std::string *out) const override;

private:
  std::string _scriptsRoot;
  lua_State *_L = nullptr;
};

} // namespace Firelands

#endif
