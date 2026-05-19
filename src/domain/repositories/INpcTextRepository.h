#pragma once

#include <domain/models/NpcText.h>
#include <cstdint>
#include <optional>

namespace Firelands {

/// Persistence for `npc_text` (gossip page copy shown in the client dialog).
class INpcTextRepository {
public:
  virtual ~INpcTextRepository() = default;

  virtual std::optional<NpcText> TryGetById(uint32_t textId) const = 0;
};

} // namespace Firelands
