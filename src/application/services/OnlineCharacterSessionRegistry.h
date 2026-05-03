#ifndef FIRELANDS_APPLICATION_SERVICES_ONLINE_CHARACTER_SESSION_REGISTRY_H
#define FIRELANDS_APPLICATION_SERVICES_ONLINE_CHARACTER_SESSION_REGISTRY_H

#include <application/ports/ICommandSession.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Firelands {

/// Maps logged-in character names (ASCII lowercased) to `ICommandSession` for
/// server-console commands that act on a specific online player.
class OnlineCharacterSessionRegistry {
public:
  void Register(std::string const &characterName, uint64_t objectGuid,
                std::weak_ptr<ICommandSession> session);
  void Unregister(std::string const &characterName, uint64_t objectGuid,
                  ICommandSession *self);

  std::shared_ptr<ICommandSession> TryResolve(std::string const &characterName) const;
  /// In-world client `ObjectGuid` (same as `WorldSession::_playerGuid` while logged in).
  std::shared_ptr<ICommandSession> TryResolveByObjectGuid(uint64_t objectGuid) const;

  /// Sorted list of registry keys (lowercased names) for sessions still online.
  std::vector<std::string> ListOnlineCharacterNames() const;

  /// `CHAT_MSG_SYSTEM`-style notification to every connected character session.
  void BroadcastSystemNotification(std::string const &message) const;
  /// System chat line plus center-screen `SMSG_NOTIFICATION` (e.g. `.announce`).
  void BroadcastAnnouncement(std::string const &chatMessage,
                             std::string const &screenMessage) const;

private:
  static std::string NormalizeName(std::string const &name);

  mutable std::mutex _mutex;
  std::unordered_map<std::string, std::weak_ptr<ICommandSession>> _byName;
  std::unordered_map<uint64_t, std::weak_ptr<ICommandSession>> _byObjectGuid;
};

} // namespace Firelands

#endif
