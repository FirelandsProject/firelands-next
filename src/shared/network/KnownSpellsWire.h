#pragma once

#include <cstdint>
#include <shared/network/WorldPacket.h>
#include <vector>

namespace Firelands::KnownSpellsWire {

/// Build 15595: legacy `SMSG_SEND_KNOWN_SPELLS` body (uint8 login flag, uint16 count,
/// uint32 spell + int16 slot pairs, uint16 empty spell-history count).
/// Bit-packed layout drops racial abilities on the 4.3.4 client.
inline void WriteSendKnownSpells(WorldPacket &packet, bool initialLogin,
                                 std::vector<uint32_t> const &knownSpellIds) {
  packet.Append<uint8_t>(initialLogin ? 1u : 0u);
  packet.Append<uint16_t>(static_cast<uint16_t>(knownSpellIds.size()));
  for (uint32_t spellId : knownSpellIds) {
    packet.Append<uint32_t>(spellId);
    packet.Append<int16_t>(0); // spell slot (unused on 15595; kept for client layout)
  }
  packet.Append<uint16_t>(0); // SpellHistoryEntries.size()
  }

} // namespace Firelands::KnownSpellsWire
