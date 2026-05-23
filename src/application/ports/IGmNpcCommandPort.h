#pragma once

#include <string>

namespace Firelands {

class IGmNpcCommandPort {
public:
  virtual ~IGmNpcCommandPort() = default;
  virtual bool GmNpcSearchPrintResults(std::string const &nameQuery) {
    (void)nameQuery;
    return false;
  }
  virtual bool GmNpcPrintTargetInfo() { return false; }
};

} // namespace Firelands
