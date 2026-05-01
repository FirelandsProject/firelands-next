#pragma once

#include <shared/Config.h>

#include <atomic>

namespace Firelands {

/// Background loop: connects to auth realm-link port and keeps TCP open.
void RunRealmLinkOutbound(const Config &config, std::atomic<bool> &stop);

} // namespace Firelands
