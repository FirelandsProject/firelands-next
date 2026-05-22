add_library(FirelandsApplication STATIC
    spell/SpellHitEffects.cpp
    spell/PassiveSpellAuras.cpp
    spell/PlayerAuraStatEffects.cpp
    spell/SpellManager.cpp
    services/PlayerCreateInfoService.cpp
    services/PlayerSpellbook.cpp
    services/CharacterActionButtons.cpp
    services/RealmListService.cpp
    services/CommandService.cpp
    services/GmTicketService.cpp
    services/OnlineCharacterSessionRegistry.cpp
    services/WorldService.cpp
)

target_include_directories(FirelandsApplication PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(FirelandsApplication PUBLIC
    FirelandsDomain
    FirelandsShared
)
target_precompile_headers(FirelandsApplication PRIVATE ${PROJECT_PCH_HEADERS})
