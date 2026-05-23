#pragma once

#include <application/ports/ICommandSessionCore.h>
#include <application/ports/IGmAppearanceCommandPort.h>
#include <application/ports/IGmCharacterCommandPort.h>
#include <application/ports/IGmFactionCommandPort.h>
#include <application/ports/IGmNpcCommandPort.h>
#include <application/ports/IGmSpellCommandPort.h>
#include <application/ports/IGmUnitCommandPort.h>

namespace Firelands {

/// World/console command execution surface (`CommandService` target).
class ICommandSession : public ICommandSessionCore,
                        public IGmSpellCommandPort,
                        public IGmCharacterCommandPort,
                        public IGmUnitCommandPort,
                        public IGmFactionCommandPort,
                        public IGmNpcCommandPort,
                        public IGmAppearanceCommandPort {};

} // namespace Firelands
