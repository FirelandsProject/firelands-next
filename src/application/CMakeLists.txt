add_library(FirelandsApplication STATIC
    combat/CombatService.cpp
    combat/CombatHostility.cpp
    combat/CreatureProximityAggro.cpp
    combat/CreatureChaseMovement.cpp
    combat/MapCombatDamage.cpp
    combat/MeleeCombatDamage.cpp
    combat/PlayerCombatStats.cpp
    spell/SpellHitEffects.cpp
    spell/SpellImpactEffects.cpp
    spell/PassiveSpellAuras.cpp
    spell/PlayerAuraStatEffects.cpp
    spell/PlayerAuraRegenEffects.cpp
    spell/SpellManager.cpp
    services/PlayerCreateInfoService.cpp
    services/PlayerSpellbook.cpp
    services/CharacterActionButtons.cpp
    services/RealmListService.cpp
    services/CommandService.cpp
    services/GmTicketService.cpp
    services/OnlineCharacterSessionRegistry.cpp
    services/WorldService.cpp
    world/PlayerPhaseShift.cpp
    world/PhaseConditionEvaluator.cpp
    world/PhaseGroupCatalog.cpp
    world/PhaseAreaCatalog.cpp
    world/PlayerQuestProgressStore.cpp
    world/WorldRuntimeAccess.cpp
    gm/GmPlayerRevive.cpp
)

target_include_directories(FirelandsApplication PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(FirelandsApplication PUBLIC
    FirelandsDomain
    FirelandsShared
)
target_precompile_headers(FirelandsApplication PRIVATE ${PROJECT_PCH_HEADERS})
