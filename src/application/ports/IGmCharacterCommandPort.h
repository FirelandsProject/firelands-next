#pragma once

#include <shared/Common.h>
#include <cstdint>

namespace Firelands {

class IGmCharacterCommandPort {
public:
  virtual ~IGmCharacterCommandPort() = default;
  virtual bool GmModifyMoneyCopper(int64 deltaCopper) {
    (void)deltaCopper;
    return false;
  }
  virtual bool GmAddItem(uint32 itemEntry, uint32 count) {
    (void)itemEntry;
    (void)count;
    return false;
  }
  virtual bool GmRemoveItem(uint32 itemEntry, uint32 count) {
    (void)itemEntry;
    (void)count;
    return false;
  }
  virtual bool GmSetLevel(uint8 level) {
    (void)level;
    return false;
  }
  virtual bool GmRevivePlayer(uint64 playerGuid) {
    (void)playerGuid;
    return false;
  }
  virtual bool GmReviveSelf() { return false; }
};

} // namespace Firelands
