#ifndef FIRELANDS_SHARED_GAME_PERMISSION_NAMES_H
#define FIRELANDS_SHARED_GAME_PERMISSION_NAMES_H

#include <shared/game/Permissions.h>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace Firelands {

struct PermissionNameEntry {
  std::string_view name;
  Permission permission;
};

/// Stable names for `.rbac` CLI and help text (lowercase, no spaces).
inline std::vector<PermissionNameEntry> const &AllNamedPermissions() {
  static std::vector<PermissionNameEntry> const kEntries = {
      {"gps", Permission::CommandGps},
      {"teleport", Permission::CommandTeleport},
      {"moderatechat", Permission::ModerateChat},
      {"manageplayers", Permission::ManagePlayers},
      {"manageaccounts", Permission::ManageAccounts},
      {"servercontrol", Permission::ServerControl},
      {"gmtools", Permission::CommandGmTools},
      {"gameplay", Permission::CommandGameplay},
      {"gmtickets", Permission::ManageGmTickets},
      {"mailbox", Permission::CommandMailbox},
  };
  return kEntries;
}

inline std::optional<Permission> PermissionFromName(std::string_view name) {
  for (auto const &e : AllNamedPermissions()) {
    if (name == e.name)
      return e.permission;
  }
  return std::nullopt;
}

/// Parses permission names and/or `0x` hex masks into a single bitmask.
inline std::optional<PermissionMask>
ParsePermissionTokens(std::vector<std::string> const &tokens) {
  PermissionMask mask = 0;
  for (std::string const &tok : tokens) {
    if (tok.size() >= 2 && (tok[0] == '0') &&
        (tok[1] == 'x' || tok[1] == 'X')) {
      try {
        mask |= static_cast<PermissionMask>(std::stoull(tok, nullptr, 0));
      } catch (...) {
        return std::nullopt;
      }
      continue;
    }
    std::string lower = tok;
    for (char &c : lower)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto perm = PermissionFromName(lower);
    if (!perm)
      return std::nullopt;
    mask |= ToMask(*perm);
  }
  return mask;
}

inline std::string FormatPermissionMask(PermissionMask mask) {
  if (mask == 0)
    return "0";
  std::string out = "0x" + [&] {
    std::ostringstream oss;
    oss << std::hex << mask;
    return oss.str();
  }();
  for (auto const &e : AllNamedPermissions()) {
    PermissionMask const bit = ToMask(e.permission);
    if ((mask & bit) == bit) {
      out += ' ';
      out += e.name;
      mask &= ~bit;
    }
  }
  if (mask != 0)
    out += " +unknown_bits";
  return out;
}

} // namespace Firelands

#endif // FIRELANDS_SHARED_GAME_PERMISSION_NAMES_H
