#include <infrastructure/network/sessions/WorldSession.h>
#include <shared/dbc/AreaTableDbc.h>

namespace Firelands {

uint32_t WorldSession::ResolveSessionAreaId(uint32_t clientAreaHint) const {
  if (_mapId == 0 || clientAreaHint == 0)
    return clientAreaHint;
  if (auto table = runtime().GetAreaTableDbc()) {
    if (table->IsLoaded())
      return table->ResolveAreaForPhasing(_mapId, clientAreaHint);
  }
  return clientAreaHint;
}

void WorldSession::SetSessionAreaId(uint32_t clientAreaHint) {
  uint32_t const resolved = ResolveSessionAreaId(clientAreaHint);
  if (resolved != 0 && resolved != _zoneId)
    _zoneId = resolved;
  if (resolved == _areaId)
    return;
  _areaId = resolved;
  if (_playerGuid != 0)
    RefreshPlayerPhaseVisibilityFromAuras();
}

} // namespace Firelands
