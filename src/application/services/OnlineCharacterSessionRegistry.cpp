#include "OnlineCharacterSessionRegistry.h"
#include <algorithm>
#include <cctype>

namespace Firelands {

std::string OnlineCharacterSessionRegistry::NormalizeName(std::string const &name) {
  std::string out;
  out.reserve(name.size());
  for (unsigned char c : name) {
    if (c <= 127)
      out.push_back(static_cast<char>(std::tolower(c)));
    else
      out.push_back(static_cast<char>(c));
  }
  return out;
}

void OnlineCharacterSessionRegistry::Register(std::string const &characterName,
                                              uint64_t objectGuid,
                                              std::weak_ptr<ICommandSession> session) {
  std::string const key = NormalizeName(characterName);
  if (key.empty())
    return;
  std::lock_guard<std::mutex> lock(_mutex);
  _byName[key] = session;
  if (objectGuid != 0)
    _byObjectGuid[objectGuid] = session;
}

void OnlineCharacterSessionRegistry::Unregister(std::string const &characterName,
                                                uint64_t objectGuid,
                                                ICommandSession *self) {
  if (!self)
    return;
  std::string const key = NormalizeName(characterName);
  std::lock_guard<std::mutex> lock(_mutex);
  if (!key.empty()) {
    auto it = _byName.find(key);
    if (it != _byName.end()) {
      if (auto locked = it->second.lock()) {
        if (locked.get() == self)
          _byName.erase(it);
      } else {
        _byName.erase(it);
      }
    }
  }
  if (objectGuid != 0) {
    auto git = _byObjectGuid.find(objectGuid);
    if (git != _byObjectGuid.end()) {
      if (auto locked = git->second.lock()) {
        if (locked.get() == self)
          _byObjectGuid.erase(git);
      } else {
        _byObjectGuid.erase(git);
      }
    }
  }
}

std::shared_ptr<ICommandSession>
OnlineCharacterSessionRegistry::TryResolve(std::string const &characterName) const {
  std::string const key = NormalizeName(characterName);
  if (key.empty())
    return nullptr;
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _byName.find(key);
  if (it == _byName.end())
    return nullptr;
  return it->second.lock();
}

std::shared_ptr<ICommandSession>
OnlineCharacterSessionRegistry::TryResolveByObjectGuid(uint64_t objectGuid) const {
  if (objectGuid == 0)
    return nullptr;
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _byObjectGuid.find(objectGuid);
  if (it == _byObjectGuid.end())
    return nullptr;
  return it->second.lock();
}

std::vector<std::string>
OnlineCharacterSessionRegistry::ListOnlineCharacterNames() const {
  std::vector<std::string> names;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    names.reserve(_byName.size());
    for (auto const &kv : _byName) {
      if (kv.second.lock())
        names.push_back(kv.first);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

void OnlineCharacterSessionRegistry::BroadcastSystemNotification(
    std::string const &message) const {
  std::vector<std::shared_ptr<ICommandSession>> sessions;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    sessions.reserve(_byName.size());
    for (auto const &kv : _byName) {
      if (auto s = kv.second.lock())
        sessions.push_back(std::move(s));
    }
  }
  for (auto const &s : sessions)
    s->SendNotification(message);
}

void OnlineCharacterSessionRegistry::BroadcastAnnouncement(
    std::string const &chatMessage, std::string const &screenMessage) const {
  std::vector<std::shared_ptr<ICommandSession>> sessions;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    sessions.reserve(_byName.size());
    for (auto const &kv : _byName) {
      if (auto s = kv.second.lock())
        sessions.push_back(std::move(s));
    }
  }
  for (auto const &s : sessions) {
    s->SendNotification(chatMessage);
    s->SendScreenNotification(screenMessage);
  }
}

} // namespace Firelands
