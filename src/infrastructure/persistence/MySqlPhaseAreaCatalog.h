#pragma once

namespace sql {
class Connection;
}

namespace Firelands {

class PhaseAreaCatalog;

void LoadPhaseAreaCatalogFromConnection(sql::Connection &connection,
                                         PhaseAreaCatalog &out);

} // namespace Firelands
