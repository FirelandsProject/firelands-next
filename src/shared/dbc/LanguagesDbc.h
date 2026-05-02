#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

namespace Firelands {

/// `Languages.dbc` (client 4.3.4.15595): one row per `Language` id + localized name
/// pointer. Used to validate wire language values against client data.
class LanguagesDbc {
public:
  bool Load(std::string const &path);

  bool IsLoaded() const { return m_loaded; }

  /// True if `languageId` appears as a row ID in the DBC. When not loaded,
  /// returns true (permissive) so chat still works without the file.
  bool HasLanguageId(uint32_t languageId) const;

private:
  bool m_loaded = false;
  std::unordered_set<uint32_t> m_languageIds;
};

} // namespace Firelands
