#pragma once

#include <domain/models/NpcText.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>

namespace Firelands::gossip {

/// Build `SMSG_NPC_TEXT_UPDATE` — matches TCPP `HandleNpcTextQueryOpcode` layout.
inline WorldPacket BuildNpcTextUpdate(NpcText const &text) {
  WorldPacket out(SMSG_NPC_TEXT_UPDATE, 256);
  out.Append<uint32_t>(text.id);

  for (NpcTextOption const &option : text.options) {
    out.Append<float>(option.probability);
    out.WriteString(option.text0);
    out.WriteString(option.text1);
    out.Append<uint32_t>(static_cast<uint32_t>(option.language));
    for (NpcTextEmote const &emote : option.emotes) {
      out.Append<uint32_t>(static_cast<uint32_t>(emote.delay));
      out.Append<uint32_t>(static_cast<uint32_t>(emote.emote));
    }
  }
  return out;
}

/// Convenience when no DB row exists.
inline WorldPacket BuildNpcTextUpdate(uint32_t textId,
                                      std::string const &greeting = "Greetings $N") {
  return BuildNpcTextUpdate(NpcText::MakeFallback(textId, greeting));
}

} // namespace Firelands::gossip
