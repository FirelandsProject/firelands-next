#pragma once

namespace sql {
class Connection;
}

namespace Firelands {

class PhaseGroupCatalog;

void LoadPhaseGroupCatalogFromConnection(sql::Connection &connection,
                                         PhaseGroupCatalog &out);

} // namespace Firelands
