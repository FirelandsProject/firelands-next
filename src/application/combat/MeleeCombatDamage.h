#pragma once

#include <shared/Common.h>
#include <domain/combat/entities/ICombatEntity.h>

namespace application {

Firelands::uint32 ComputeMeleeDamageBetween(::combat::ICombatEntity const &attacker,
                                            ::combat::ICombatEntity const &victim);

} // namespace application
