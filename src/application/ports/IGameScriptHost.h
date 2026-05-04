#ifndef FIRELANDS_APPLICATION_PORTS_I_GAME_SCRIPT_HOST_H
#define FIRELANDS_APPLICATION_PORTS_I_GAME_SCRIPT_HOST_H

#include <cstdint>
#include <memory>
#include <string>

namespace Firelands {

class FactionTemplateDbc;

/// Port for gameplay scripting (Lua implementation lives in infrastructure).
/// Keeps domain/world code free of script engine types.
class IGameScriptHost {
public:
  virtual ~IGameScriptHost() = default;

  /// Creates the Lua state and optionally loads `bootstrap.lua` from
  /// `scriptsRoot` when that directory exists.
  virtual bool Init(const std::string &scriptsRoot) = 0;

  virtual void Shutdown() = 0;

  /// Runs Lua source as a chunk. On failure, writes a message into `errorOut`
  /// when non-null.
  virtual bool RunChunk(const std::string &source, std::string *errorOut) = 0;

  /// Invokes global `OnScriptEvent(eventName, contextGuid)` when it is a
  /// function. `contextGuid` may be zero when not applicable.
  virtual void FireEvent(const std::string &eventName,
                           uint64_t contextGuid = 0) = 0;

  /// NPC gossip open (maps to Lua `gossip_hello` via `OnScriptEvent`).
  virtual void FireGossipHello(uint64_t npcGuid) = 0;

  /// Gossip menu option chosen; Lua may read globals `_gossipMenuId` and
  /// `_gossipListId` during `OnScriptEvent("gossip_select", ...)`.
  virtual void FireGossipSelect(uint64_t npcGuid, uint32_t menuId,
                                uint32_t gossipListId) = 0;

  /// Reads a global as UTF-8 string (for tests and admin tooling).
  virtual bool TryGetGlobalString(const std::string &globalName,
                                  std::string *out) const = 0;

  /// Optional: after `FactionTemplate.dbc` is loaded, registers Lua helpers (`firelands_*`).
  virtual void AttachFactionTemplateDbc(
      std::shared_ptr<FactionTemplateDbc const> store) {
    (void)store;
  }
};

} // namespace Firelands

#endif
