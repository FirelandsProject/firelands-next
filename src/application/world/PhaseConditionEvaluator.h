#pragma once

#include <domain/ports/IPlayerQuestProgress.h>
#include <domain/models/PhaseCondition.h>

namespace Firelands {

/// Returns true when `conditions` is empty or any ElseGroup matches (TrinityCore rules).
bool EvaluatePhaseConditions(PhaseConditionList const &conditions,
                             IPlayerQuestProgress const &player);

} // namespace Firelands
