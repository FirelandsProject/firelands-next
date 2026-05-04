#ifndef FIRELANDS_INFRASTRUCTURE_SCRIPTING_LUA_GAME_SCRIPT_HOST_H
#define FIRELANDS_INFRASTRUCTURE_SCRIPTING_LUA_GAME_SCRIPT_HOST_H

#include <application/ports/IGameScriptHost.h>
#include <memory>
#include <string>

struct lua_State;

namespace Firelands {

class FactionTemplateDbc;

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
  void FireGossipHello(uint64_t npcGuid) override;
  void FireGossipSelect(uint64_t npcGuid, uint32_t menuId,
                        uint32_t gossipListId) override;
  bool TryGetGlobalString(const std::string &globalName,
                          std::string *out) const override;

  void AttachFactionTemplateDbc(
      std::shared_ptr<FactionTemplateDbc const> store) override;

  std::shared_ptr<FactionTemplateDbc const> GetFactionTemplateDbc() const {
    return _factionTemplateDbc;
  }

private:
  void RegisterFactionLuaApi();

  std::string _scriptsRoot;
  lua_State *_L = nullptr;
  std::shared_ptr<FactionTemplateDbc const> _factionTemplateDbc;
};

} // namespace Firelands

#endif
