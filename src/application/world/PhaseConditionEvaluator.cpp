#include <application/world/PhaseConditionEvaluator.h>

#include <domain/models/QuestProgress.h>

#include <map>

namespace Firelands {

namespace {

bool EvaluateSingleCondition(PhaseCondition const &condition,
                             IPlayerQuestProgress const &player) {
  bool meets = false;
  switch (condition.type) {
  case PhaseConditionType::None:
    meets = true;
    break;
  case PhaseConditionType::Aura:
    meets = player.HasAuraSpell(condition.value1);
    break;
  case PhaseConditionType::QuestRewarded:
    meets = player.IsQuestRewarded(condition.value1);
    break;
  case PhaseConditionType::QuestTaken:
    meets = player.GetQuestStatus(condition.value1) == QuestStatus::Incomplete;
    break;
  case PhaseConditionType::QuestComplete:
    meets = player.GetQuestStatus(condition.value1) == QuestStatus::Complete &&
            !player.IsQuestRewarded(condition.value1);
    break;
  default:
    meets = false;
    break;
  }

  if (condition.negative)
    meets = !meets;
  return meets;
}

} // namespace

bool EvaluatePhaseConditions(PhaseConditionList const &conditions,
                             IPlayerQuestProgress const &player) {
  if (conditions.empty())
    return true;

  std::map<uint32, bool> elseGroups;
  for (PhaseCondition const &condition : conditions) {
  auto const it = elseGroups.find(condition.elseGroup);
    if (it != elseGroups.end() && !it->second)
      continue;

    if (!EvaluateSingleCondition(condition, player))
      elseGroups[condition.elseGroup] = false;
    else if (it == elseGroups.end())
      elseGroups.emplace(condition.elseGroup, true);
  }

  for (auto const &[_, passed] : elseGroups) {
    if (passed)
      return true;
  }
  return false;
}

} // namespace Firelands
