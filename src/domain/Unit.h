#ifndef FIRELANDS_DOMAIN_UNIT_H
#define FIRELANDS_DOMAIN_UNIT_H

#include <shared/Common.h>
#include <algorithm>

namespace Firelands {

    class Unit {
    public:
        explicit Unit(uint32 maxHealth) 
            : _maxHealth(maxHealth), _health(maxHealth) {}

        uint32 GetHealth() const { return _health; }
        uint32 GetMaxHealth() const { return _maxHealth; }

        void SetHealth(uint32 health) {
            _health = std::min(health, _maxHealth);
        }

    private:
        uint32 _maxHealth;
        uint32 _health;
    };

} // namespace Firelands

#endif // FIRELANDS_DOMAIN_UNIT_H
