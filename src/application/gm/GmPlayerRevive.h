#pragma once

#include <shared/Common.h>
#include <cstdint>

namespace Firelands {

class Player;

enum class GmReviveResult {
  AlreadyFull,
  Restored,
  RevivedFromDeath,
};

struct GmReviveOutcome {
  GmReviveResult result = GmReviveResult::AlreadyFull;
  bool healthChanged = false;
  bool powerChanged = false;
};

/// Restores live HP/POWER1 toward max; does not send packets (session broadcasts).
GmReviveOutcome ApplyGmReviveToPlayer(Player &player);

} // namespace Firelands
