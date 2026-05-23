#include "CombatEngine.h"

namespace combat {
    void CombatEngine::Engage(ICombatEntity& attacker, ICombatEntity& victim) {
        if (!attacker.IsAlive() || !victim.IsAlive()) return;
        _threatMgr->AddThreat(victim.GetGuid(), attacker.GetGuid(), 1.0f);
    }

    void CombatEngine::Update(ICombatEntity& attacker, ICombatEntity& victim) {
        if (!attacker.IsAlive() || !victim.IsAlive()) return;
        float dmg = _dmgCalc.Calculate(10.0f, 1.0f);
        victim.TakeDamage(dmg);
    }

    void CombatEngine::ApplyMeleeDamage(ICombatEntity& victim, float amount) {
        if (!victim.IsAlive() || amount <= 0.f)
            return;
        victim.TakeDamage(amount);
    }

    void CombatEngine::HandleSpell(ICombatEntity& attacker, uint64_t spellId, ICombatEntity& target) {
        if (_spellProc->CanCast(spellId, target.GetGuid())) {
            _spellProc->ExecuteCast(spellId, target.GetGuid());
        }
    }
}
