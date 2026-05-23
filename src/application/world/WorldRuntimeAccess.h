#pragma once

#include <application/ports/IWorldRuntime.h>
#include <memory>

namespace Firelands {

/// Process-wide runtime (backed by `WorldService` today).
IWorldRuntime &WorldRuntime();
std::shared_ptr<IWorldRuntime> WorldRuntimePtr();

} // namespace Firelands
