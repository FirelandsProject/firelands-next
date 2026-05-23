#pragma once

#include <shared/Common.h>
#include <cstdint>

namespace Firelands {

class IGmSpellCommandPort {
public:
  virtual ~IGmSpellCommandPort() = default;
  virtual bool GmLearnSpell(uint32 spellId) {
    (void)spellId;
    return false;
  }
  virtual bool GmUnlearnSpell(uint32 spellId) {
    (void)spellId;
    return false;
  }
  virtual bool GmResetAllCooldowns() { return false; }
};

} // namespace Firelands
