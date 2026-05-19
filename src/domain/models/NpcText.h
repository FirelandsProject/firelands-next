#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace Firelands {

/// Trinity `MAX_NPC_TEXT_OPTIONS` (Cataclysm).
inline constexpr std::size_t kNpcTextOptionCount = 8;
/// Emotes per text option in `SMSG_NPC_TEXT_UPDATE`.
inline constexpr std::size_t kNpcTextEmoteCount = 3;

struct NpcTextEmote {
  uint16_t delay = 0;
  uint16_t emote = 0;
};

/// One gossip text slot (`textN_0` / `textN_1`, probability, language, emotes).
struct NpcTextOption {
  float probability = 0.f;
  std::string text0;
  std::string text1;
  uint8_t language = 0;
  std::array<NpcTextEmote, kNpcTextEmoteCount> emotes{};
};

/// NPC gossip page (`npc_text` row).
struct NpcText {
  uint32_t id = 0;
  std::array<NpcTextOption, kNpcTextOptionCount> options{};

  /// Placeholder when `npc_text` has no row (client still expects eight slots).
  static NpcText MakeFallback(uint32_t textId,
                              std::string const &greeting = "Greetings $N");
};

inline NpcText NpcText::MakeFallback(uint32_t textId, std::string const &greeting) {
  NpcText out;
  out.id = textId;
  out.options[0].probability = 1.f;
  out.options[0].text0 = greeting;
  out.options[0].text1 = greeting;
  return out;
}

} // namespace Firelands
