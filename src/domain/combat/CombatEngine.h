#pragma once
#include "entities/ICombatEntity.h"
#include "DamageCalculator.h"
#include "repositories/IThreatManager.h"
#include "repositories/ISpellProcessor.h"
#include <memory>

namespace combat {
    class CombatEngine {
    public:
        CombatEngine(std::shared_ptr<IThreatManager> threatMgr, std::shared_ptr<ISpellProcessor> spellProc)
            : _threatMgr(threatMgr), _spellProc(spellProc) {}

        void Engage(ICombatEntity& attacker, ICombatEntity& victim);
        void Update(ICombatEntity& attacker, ICombatEntity& victim);
        void ApplyMeleeDamage(ICombatEntity& victim, float amount);
        void HandleSpell(ICombatEntity& attacker, uint64_t spellId, ICombatEntity& target);

    private:
        std::shared_ptr<IThreatManager> _threatMgr;
        std::shared_ptr<ISpellProcessor> _spellProc;
        DamageCalculator _dmgCalc;
    };
}
